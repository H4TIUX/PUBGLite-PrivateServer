/*
Copyright (C) 2026 H4TIUX & Phikill

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published
by the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program. If not, see <https://www.gnu.org/licenses/>.
*/

#include "dummy.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <mutex>
#include <random>
#include <unordered_map>
#include <vector>

#include "SDK.hpp"
#include <unordered_set> 
  
#include "SDK/ShadowTrackerExtra_parameters.hpp"

#define PI 3.14159265358979323846f

using namespace SDK;

bool bWasInAirplane = false;

int UMP_SPAWN = 0;
int cloth_spawn = 0;
int GMapId = 0;

static volatile LONG g_PendingAppliesCount = 0;

volatile bool itemADDED = 0;
volatile bool HOST_UMP_SPAWN = 0;
volatile bool client_character_change = 0;

volatile bool bot_follow_test = 0;

volatile bool serverPanelInitialized = 0;

int gLastTeamID = 0;

SDK::ASTExtraBaseCharacter* GetCharacterFromDController(SDK::APlayerController* PC)
{
    if (!PC) return nullptr;
    if (PC->K2_GetPawn() && PC->K2_GetPawn()->IsA(SDK::ASTExtraBaseCharacter::StaticClass()))
        return static_cast<SDK::ASTExtraBaseCharacter*>(PC->K2_GetPawn());
    return nullptr;
}

struct PlayerInfo
{
    SDK::APlayerState* PS;
    std::string Name;
    int UID;
    SDK::ASTExtraBaseCharacter* Character;

    bool bInVehicle = false;
    SDK::ASTExtraVehicleBase* Vehicle = nullptr;
    int VehicleSeatIdx = -1;
};

struct FReloadData
{
    bool bWaitingForReload = false;
    bool bWasInAirplane = true;
    bool bWasWeaponFiring;
    bool LastWasHeadshot;

    UTslBallisticsComp* LastBallistics = nullptr;
    ASTExtraBaseCharacter* LastCharacter = nullptr;
    UCharacterInteractionComponent* LastInteraction = nullptr;
    ASTExtraShootWeapon* LastShootWeapon = nullptr;
    UVehicleUserComponent* LastVehicleComp = nullptr;
    FSTPointDamageEvent LastPointDamageEvent{};

    std::chrono::steady_clock::time_point ReloadFinishTime;
};

class AActor* SpawnActorFromClass(
    class UObject* WorldContextObject, TSubclassOf < class AActor > ActorClass,
    const struct FTransform& SpawnTransform,
    ESpawnActorCollisionHandlingMethod CollisionHandlingOverride,
    class AActor* Owner) {

    /*
    auto Spawned = UGameplayStatics::BeginDeferredActorSpawnFromClass(
        WorldContextObject, ActorClass, SpawnTransform, CollisionHandlingOverride,
        Owner);
        */

    auto Spawned = UGameplayStatics::BeginSpawningActorFromClass(WorldContextObject, ActorClass, SpawnTransform, 0, Owner);

    Spawned = UGameplayStatics::FinishSpawningActor(Spawned, SpawnTransform);

    return Spawned;
}

struct FHookInfo
{
    void* HookedObject = nullptr;
    void* OriginalVTable = nullptr;
};

std::unordered_map<UObject*, FHookInfo> Hooks;
std::unordered_map<void*, FReloadData> ReloadingPlayers;
std::vector<UObject*> HookedVTables;

UC::TArray<PlayerInfo> RegisteredPlayers;

FTransform SpawnTransform{};
auto AI = (AAIController*)SpawnActorFromClass(UWorld::GetWorld(), AAIController::StaticClass(), SpawnTransform, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn, nullptr);

void ClearAllHooks()
{
    for (auto& [Object, Hook] : Hooks)
    {
        if (Object)
        {
            *(void**)Object = Hook.OriginalVTable;
        }

        delete[] Hook.HookedObject;
    }

    Hooks.clear();
}

bool IsNearGround(SDK::ASTExtraPlayerController* STEPC, float MaxDistance)
{
    if (!STEPC || !STEPC->STExtraBaseCharacter)
        return false;

    SDK::UWorld* World = SDK::UWorld::GetWorld();
    if (!World)
        return false;

    ASTExtraBaseCharacter* Char = STEPC->STExtraBaseCharacter;

    // ponto central do personagem
    SDK::FVector Start = Char->K2_GetActorLocation();

    // desce no eixo Z
    SDK::FVector End = Start - SDK::FVector(0.f, 0.f, MaxDistance + 50.f);

    SDK::FHitResult Hit;

    SDK::TArray<SDK::AActor*> Ignore;
    Ignore.Add(Char);

    const float Radius = 20.f; // "pé" do character

    // profile de colisão (mais estável que TraceTypeQuery)
    //SDK::FName ProfileName(L"BlockAll");
    SDK::FName ProfileName;
    ProfileName.ComparisonIndex = 0; // BlockAll costuma ser 0
    ProfileName.Number = 0;

    bool bHit = SDK::UKismetSystemLibrary::SphereTraceSingleByProfile(World,
        Start,
        End,
        Radius,
        ProfileName,
        false,                      // bTraceComplex
        Ignore,
        SDK::EDrawDebugTrace::None,
        &Hit,
        true,                       // bIgnoreSelf
        SDK::FLinearColor(),
        SDK::FLinearColor(),
        0.f);

    if (!bHit)
    {
        return false;
    }


    // distância vertical até o impacto
    float DistanceToGround = Start.Z - Hit.ImpactPoint.Z;
    return DistanceToGround <= MaxDistance;
}

bool IsHostPlayer(SDK::APlayerState* PS, SDK::UWorld* World)
{
    if (!PS || !World)
    {
        return false;
    }

    SDK::APlayerController* LocalPC = UGameplayStatics::GetPlayerController(World, 0);
    if (!LocalPC)
    {
        return false;
    }

    return PS->GetOwner() == LocalPC;
}

SDK::ASTExtraPlayerController* GetSTExtraPC(SDK::APlayerController* PC)
{
    if (!PC) return nullptr;
    if (!PC->IsA(SDK::ASTExtraPlayerController::StaticClass())) return nullptr;
    return (SDK::ASTExtraPlayerController*)PC;
}

SDK::ASTExtraBaseCharacter* GetCharacterFromController(SDK::APlayerController* PC)
{
    SDK::ASTExtraPlayerController* STPC = GetSTExtraPC(PC);
    if (!STPC) return nullptr;

    // 1) Prefer direct member if present (mais rápido, seguro)
    if (STPC->STExtraBaseCharacter)
        return STPC->STExtraBaseCharacter;

    // 2) Fallback para util (caso dumper / offsets retornem via função)
    SDK::ASTExtraBaseCharacter* fromUtil = SDK::USTExtraVehicleUtils::GetCharacter(STPC);
    if (fromUtil) return fromUtil;

    // 3) fallback final: se pawn é character
    if (PC->K2_GetPawn() && PC->K2_GetPawn()->IsA(SDK::ASTExtraBaseCharacter::StaticClass()))
        return (SDK::ASTExtraBaseCharacter*)PC->K2_GetPawn();

    return nullptr;
}

std::string GetObjectClassNameEx(SDK::UObject* Obj)
{
    if (!Obj)
    {
        return "NULL_OBJ";
    }

    if (!Obj->GetClass())
    {
        return "NULL_CLASS";
    }

    try
    {
        return Obj->GetClass()->GetName();
    }
    catch (...)
    {
        return "ERR";
    }
}

void InitGenerator()
{
    UItemGeneratorComponent* ItemGen = (UItemGeneratorComponent*)UObject::FindObjectFast("ItemGeneratorComponent");
    AUAEGameMode* UAEGM = (AUAEGameMode*)UGameplayStatics::GetGameMode(UWorld::GetWorld());

    if (ItemGen)
    {
        std::cerr << "Found item generator comp" << std::endl;

        for (UItemGroupSpotSceneComponent* GroupSpot : ItemGen->AllValidGroups)
        {
            for (UItemSpotSceneComponent* SpotScene : GroupSpot->SpotsCacheCur)
            {
                std::cerr << "Current ItemSpotScene component: " << SpotScene->GetName() << std::endl;

                SpotScene->GenerateItems(&SpotScene->AllItems);
            }
        }

        //UAEGM->InitGenerator();
    }
    else
    {
        std::cerr << "Couldn't find item generator comp" << std::endl;
    }

}

void StaticItemGeneratorFix()
{
    UBaseGeneratorComponent* BaseGen = (UBaseGeneratorComponent*)UObject::FindObjectFast("BaseGeneratorComponent");

    if (BaseGen) {
        std::cerr << "Found base generator component" << std::endl;
    }
    else {
        std::cerr << "Couldn't find base generator component" << std::endl;
    }

    for (USpotSceneComponent* TickSpot : BaseGen->AllSpotsToTick)
    {
        std::cerr << "Tick spot: " << TickSpot->GetName() << std::endl;
    }
}

void ConfigureDamageHitbox(SDK::UPrimitiveComponent* Hitbox, const char* Name)
{
    if (!Hitbox)
        return;

    bool bChanged = false;
    if (Hitbox->GetCollisionEnabled() != ECollisionEnabled::QueryOnly)
    {
        Hitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        bChanged = true;
    }

    Hitbox->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
    /*
    Hitbox->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
    Hitbox->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Ignore);
    Hitbox->SetCollisionResponseToChannel(ECollisionChannel::ECC_Vehicle, ECollisionResponse::ECR_Ignore);
    Hitbox->SetCollisionResponseToChannel(ECollisionChannel::ECC_PhysicsBody, ECollisionResponse::ECR_Ignore);
    Hitbox->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldDynamic, ECollisionResponse::ECR_Ignore);
    Hitbox->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldStatic, ECollisionResponse::ECR_Ignore);
    */

    /*
    if (!Hitbox->bReplicates)
    {
        Hitbox->SetIsReplicated(true);
        Hitbox->bReplicates = true;
        bChanged = true;
    }

    if (!Hitbox->AlwaysLoadOnClient)
    {
        Hitbox->AlwaysLoadOnClient = true;
        bChanged = true;
    }

    if (!Hitbox->AlwaysLoadOnServer)
    {
        Hitbox->AlwaysLoadOnServer = true;
        bChanged = true;
    }
    */

    if (bChanged && Name)
    {
        static DWORD LastLogTick = 0;
        DWORD Now = GetTickCount();
        if (Now - LastLogTick > 1000)
        {
            //std::cerr << "Damage hitbox enabled: " << Name << std::endl;
            LastLogTick = Now;
        }
    }
}

bool SpawnAndGiveItem(SDK::UBackpackComponent* Backpack,
    SDK::EBackpackItemType ItemType,
    int32_t SpecificID,
    int32_t Count,
    bool bAutoEquip,
    uint64_t CustomInstanceID)
{
    if (!Backpack) return false;

    SDK::UWorld* World = SDK::UWorld::GetWorld();
    SDK::AActor* Player = Backpack->GetOwner();
    if (!World || !Player) return false;

    SDK::UClass* WrapperClass = SDK::APickUpWrapperActor::StaticClass();
    if (!WrapperClass) return false;

    // 1. Defines Spawn Position
    SDK::FTransform SpawnTransform = Player->GetTransform();
    SpawnTransform.Translation.X += 150.0f; // 1.5 meters ahead
    SpawnTransform.Translation.Z += 20.0f;  // Lightly suspended

    // 2. Starts Delayed Spawn on the map
    SDK::AActor* GenericActor = SDK::UGameplayStatics::BeginDeferredActorSpawnFromClass(World,
        WrapperClass,
        SpawnTransform,
        SDK::ESpawnActorCollisionHandlingMethod::AlwaysSpawn, Player);

    if (!GenericActor) return false;

    // 3. Configures the physical floor wrapper.
    SDK::APickUpWrapperActor* PickupItem = reinterpret_cast<SDK::APickUpWrapperActor*>(GenericActor);

    SDK::FItemDefineID ItemID{};
    ItemID.Type = (int32_t)ItemType;
    ItemID.TypeSpecificID = SpecificID;
    ItemID.AvatarID = 0;
    ItemID.bValidItem = true;
    ItemID.bValidInstance = true;
    ItemID.InstanceID = CustomInstanceID;

    PickupItem->DefineID = ItemID;
    PickupItem->Count = Count;
    PickupItem->bDropedByPlayer = true;

    // 4. Physical Spawn completes
    SDK::UGameplayStatics::FinishSpawningActor(PickupItem, SpawnTransform);

    // 5. Prepare the backpack collection structure.
    SDK::FBattleItemPickupInfo PickupInfo{};
    PickupInfo.Source = PickupItem;
    PickupInfo.Count = Count;
    PickupInfo.bAutoEquip = bAutoEquip;

    // 6. Force the backpack to suck the item into the inventory.
    bool bCollected = Backpack->PickupItem(ItemID, PickupInfo, SDK::EBattleItemPickupReason::AutoPickup);
    if (bCollected)
    {
        Backpack->NotifyItemListUpdated();
        return true;
    }

    return false;
}

//using TargetFn = void(*)(UTslBallisticsComp*, const struct FServerNotifyHitArgs&);
//TargetFn OriginalFunction = nullptr;

float GetArmorReduction(int id)
{
    switch (id)
    {
    case 502001: return 0.30f;
    case 502002: return 0.40f;
    case 502003: return 0.55f;

    case 503001: return 0.30f;
    case 503002: return 0.40f;
    case 503003: return 0.55f;
    }

    return 0.f;
}

void MH_Initalize()
{
    printf("\x4F""G\072B\x41""T\124L\x45""G\122O\x55""N\104S\x2C""F\122E\x45"" \101N\x44"" \117P\x45""N\040S\x4F""U\122C\x45"" \103O\x4E""T\105N\x54"",\040D\x4F"" \116O\x54"" \123T\x45""A\114 \x4F""R\040R\x45""S\105L\x4C"".\040M\x41""D\105 \x42""Y\040H\x34""T\111U\x58"" \101N\x44"" \120H\x49""K\111L\x4C"".\040h\x74""t\160s\x3A""/\057g\x69""t\150u\x62"".\143o\x6D""/\1104\x54""I\125X\x0A""");
}

float GetDurabilityFactor(int id)
{
    switch (id)
    {
        // Helmet
    case 502001: return 0.40f;
    case 502002: return 0.45f;
    case 502003: return 0.50f;

        // Armor
    case 503001: return 0.65f;
    case 503002: return 0.70f;
    case 503003: return 0.75f;

    default: return 0.6f;
    }
}

/*

     function Addr: 0x14051d280

 */
bool UVehicleSeatComponent_IsVehiclePassengerSeat(SDK::UVehicleSeatComponent* self, int32_t SeatIndex)
{
    if (self)
    {
        if (SeatIndex >= 0 && SeatIndex < self->Seats.Num())
        {
            // if Different of Driver, returns true;
            if (self->Seats[SeatIndex].SeatType != SDK::ESTExtraVehicleSeatType::ESeatType_DriversSeat)
            {
                return true;
            }
            else
            {
                return false;
            }
        }
    }


    return false;
}

SDK::EWeaponHoldType ASTExtraWeapon_GetWeaponHoldType(SDK::ASTExtraWeapon* self);
/*

dadaadada

*/


void CleanCharacterFromAllSeats(SDK::ASTExtraVehicleBase* Vehicle, SDK::ASTExtraBaseCharacter* Character)
{
    if (!Vehicle || !Character)
    {
        return;
    }

    SDK::UVehicleSeatComponent* seats = Vehicle->VehicleSeats;
    if (!seats)
    {
        return;
    }


    for (int i = 0; i < seats->Seats.Num(); i++)
    {
        if (seats->SeatOccupiers[i] == Character)
        {
            //printf("[CLEAN] Removing character from seat %d\n", i);
            seats->SeatOccupiers[i] = nullptr;
        }
    }
}

void ChangeVehicleFuel(SDK::ASTExtraVehicleBase* TargetVehicle, float FuelValue)
{
    TargetVehicle->VehicleCommon->Fuel = FuelValue;
}

int32_t UVehicleSeatComponent_GetCharacterSeatIndex(SDK::UVehicleSeatComponent* self, SDK::ASTExtraBaseCharacter* Character)
{
    int32_t i = 0;

    if (Character == NULL)
    {
        return -1;
    }
    else
    {
        for (i = 0; i < self->SeatOccupiers.Num(); i++)
        {
            if (self->SeatOccupiers[i] == Character)
            {
                return i; // return Seat Index 
            }
        }
    }

    return -1;

}

void ASTExtraBaseCharacter_ServerUpdateVehicleSeatInfo(SDK::ASTExtraBaseCharacter* self)
{
    //ENetMode NetMode;
    int32_t CharacterVehicleSeatIndex;
    int IsBound = 0;

    //NetMode = AActor_InternalGetNetMode(this);
    //if(NetMode == ENetMode::NM_DedicatedServer)
    {
        CharacterVehicleSeatIndex = UVehicleSeatComponent_GetCharacterSeatIndex(self->CurrentVehicle->VehicleSeats, self);
        //IsBound = (self->OnChangedVehicleSeat).InvocationList.Num;
        self->VehicleSeatIdx = CharacterVehicleSeatIndex;
        /* OnChangedVehicleSeat.IsBound() */
        if (0 < IsBound)
        {
            /* void FUN_14029fb10(UMulticastDelegateProperty_ *param_1,undefined8 param_2)
               .
               .
               OnChangedVehicleSeat.Broadcast(); */
               //ProcessMulticastDelegate(&this->OnChangedVehicleSeat, 0);
            return;
        }
    }
    return;
}



std::string GetPUBGLiteNetID(SDK::APlayerState* TargetPlayerState)
{
    if (!TargetPlayerState)
        return "";

    // 1. Aponta para a struct UniqueId (FUniqueNetIdRepl)
    SDK::FUniqueNetIdRepl* repl = &TargetPlayerState->UniqueId;
    if (!repl)
        return "";

    // 2. Cast em nível de memória para interpretar os ponteiros internos (TSharedPtr)
    void** raw = (void**)repl;

    // raw[0] = VTable/Metadata no .text (0x14...)
    // raw[1] = Objeto FUniqueNetIdString alocado na Heap (0x27...)
    void* realUidObject = raw[1];

    if (!realUidObject)
        return "";

    // 3. Acessa a FString nativa que está gravada no offset 0x18 do objeto
    SDK::FString* netIdStr = (SDK::FString*)((uintptr_t)realUidObject + 0x18);

    if (netIdStr && netIdStr->IsValid())
    {
        return netIdStr->ToString();
    }

    return "";
}

bool bot_character_change = 0;

void (*ServerNotifyHitFn)(UTslBallisticsComp*, const struct FServerNotifyHitArgs&) = 0;
void (*ServerNotifyHitSimpleFn)(UTslBallisticsComp*, const struct FServerNotifyHitArgs_Simple&) = 0;
void (*ServerNotifyHitNPCFn)(UTslBallisticsComp*, const struct FServerNotifyHitArgs_NonPlayerCharacter&) = 0;
void* (*ProcessEventO)(UObject* Obj, UFunction* Func, void* Func_Params) = nullptr;

TSubclassOf<class UDamageType> LastDamageType = nullptr;
FAttackId LastAttackId{};

void HookProcessEventForCharacter(UObject* Object);
void HookProcessEventForPlayerController(UObject* Object);

void OnStartMap(int MapId);
void FixValue();
void Fix_Character_Logic(ASTExtraBaseCharacter* Character);

static int Team1Count = 0;
static int Team2Count = 0;

static int NextTeamID = 1;

void* ProcessEventHook(UObject* Obj, UFunction* Func, void* Func_Params)
{
    if (Obj && Func)
    {
        auto ObjName = Obj->GetName();
        auto FuncName = Func->GetName();

        if (FuncName != "ReceiveTick" && FuncName != "MustSpectate")
        {
            std::cerr << ObjName << " CALLED " << FuncName << std::endl;
        }

        /*
        if ((Obj->IsA(AInfo::StaticClass()) or Obj->IsA(ASTExtraGameMode::StaticClass()) and FuncName != "ReceiveTick") and FuncName != "BlueprintUpdateCamera"
            and not(FuncName.contains("BndEvt") or FuncName.contains("ReceiveHit") or FuncName.contains("ReceiveDrawHUD") or FuncName.contains("ConstructionScript") or FuncName.contains("ServerUpdateCamera") or FuncName.contains("OnParameterUpdated") or FuncName == "ReceiveBeginPlay")) {
            std::cerr << ObjName << " CALLED " << FuncName << std::endl;
        }
        */

        if (Func->GetName() == "K2_OnRestartPlayer")
        {
            printf("\n ClientRestart() From ProcessEvent \n");

            struct Params_org {
                class AController* NewPlayer;
            };

            auto Params = (Params_org*)Func_Params;

            Fix_Character_Logic((ASTExtraBaseCharacter*)Params->NewPlayer->K2_GetPawn());

            APawn* Pawn = Params->NewPlayer->K2_GetPawn(); 

            if (UGameplayStatics::GetGameMode(UWorld::GetWorld())->IsA(ATeamMatchGameMode::StaticClass()) && UGameplayStatics::GetCurrentLevelName(UWorld::GetWorld(), 1).ToString().contains("Bodie"))
            {
                std::cerr << "Bodie" << std::endl; 

                ATeamMatchGameMode* GM = (ATeamMatchGameMode*)UGameplayStatics::GetGameMode(UWorld::GetWorld());
                ATeamMatchGameState* GS = (ATeamMatchGameState*)GM->GameState;

                GM->TeamSize = 5;
                GS->NumTeams = 2;
            }

            if (Params != NULL)
            {
                ASTExtraBaseCharacter* Character = (ASTExtraBaseCharacter*)Params->NewPlayer->K2_GetPawn();

                if (Character->InteractionComponent && Character->InteractionComponent != ReloadingPlayers[Character].LastInteraction && std::find(HookedVTables.begin(), HookedVTables.end(), Character) == HookedVTables.end() &&
                    ReloadingPlayers[Character].LastInteraction != Character->InteractionComponent)
                {
                    std::cerr << "Hooking InteractionComponent..." << std::endl;

                    ReloadingPlayers[Character].LastInteraction = Character->InteractionComponent;

                    HookProcessEventForCharacter(Character->InteractionComponent);
                    HookProcessEventForCharacter(Character->InteractorComponent);
                    //HookProcessEventForPlayerController(Params->NewPlayer->CastToPlayerController());

                    HookedVTables.push_back(Character->InteractionComponent);
                    HookedVTables.push_back(Character->InteractorComponent);
                }
            }
        }

        if (Func->GetName() == "K2_OnSetMatchState")
        {
            ATslLPCGameMode* GameMode = (ATslLPCGameMode*)UGameplayStatics::GetGameMode(UWorld::GetWorld());
            std::cerr << "GameMode set new MatchState: " << GameMode->MatchState.ToString() << std::endl;

            printf("\x54""H\111S\x20""I\123 \x46""R\105E\x20""C\117N\x54""E\116T\x20""B\131 \x4F""G\072B\x41""T\124L\x45""G\122O\x55""N\104S\x2C"" \111F\x20""Y\117U\x20""B\117U\x47""H\124 \x54""H\111S\x2C"" \131O\x55"" \110A\x56""E\040B\x45""E\116 \x53""C\101M\x4D""E\104.\x20""h\164t\x70""s\072/\x2F""g\151t\x68""u\142.\x63""o\155/\x48""4\124I\x55""X\012");

            if (GameMode->MatchState.ToString() == "InProgress")
            {
                std::cerr << "Match started." << std::endl;
            }

            if (GameMode->MatchState.ToString() == "WaitingPostMatch")
            {
                std::cerr << "Match finished, waiting time and restarting match..." << std::endl;
                std::cerr << "PostMatchWaitingTime: " << GameMode->PostMatchWaitingTime << std::endl; // Doesn't work lol

                Sleep(8000);

                __fastfail(0);
            }
        }

        if (Func->GetName() == "ReqChangeVehicleSeatForPC")
        {
            std::cerr << "ReqChangeVehicleSeatForPC" << std::endl; 

            UVehicleUserComponent* _this = (UVehicleUserComponent*)Obj;
            auto Parms = (Params::VehicleUserComponent_ReqChangeVehicleSeatForPC*)Func_Params;

            auto SeatIndex = Parms->SeatIndex;

            if (!_this)
            {
                printf("[ReqChangeVehicleSeatForPC] this NULL\n");
                return ProcessEventO(Obj, Func, Func_Params);
            }

            printf("[ReqChangeVehicleSeatForPC] SeatIndex: %d | this: %p\n", SeatIndex, _this);

            SDK::ASTExtraVehicleBase* State_Vehicle = _this->Vehicle;

            SDK::ASTExtraBaseCharacter* STExtraBaseCharacter = (SDK::ASTExtraBaseCharacter*)_this->Character;
            if (STExtraBaseCharacter)
            {
                int CurrentSeatOccupiersNum = State_Vehicle->VehicleSeats->SeatOccupiers.Num();
                if (SeatIndex < CurrentSeatOccupiersNum)
                {
                    STExtraBaseCharacter->VehicleSeatIdx = SeatIndex;
                }
            }

            return ProcessEventO(_this, Func, Func_Params);
        }

        if (Func->GetName() == "ReqEnterVehicle")
        {
            std::cerr << "ReqEnterVehicle" << std::endl; 
        }

        if (Func->GetName() == "ReqExitVehicle")
        {
            std::cerr << "ReqExitVehicle" << std::endl;

            UVehicleUserComponent* _this = (UVehicleUserComponent*)Obj;
            auto Params = (Params::VehicleUserComponent_ReqExitVehicle*)Func_Params;
            FVector Velocity = Params->ClientVehicleVelocity;

            if (!_this || !_this->Vehicle || !_this->Character)
            {
                std::cerr << "ReqExitVehicle failed" << std::endl;
                return ProcessEventO(Obj, Func, Func_Params);
            }


            SDK::ASTExtraBaseCharacter* baseCharacter = (SDK::ASTExtraBaseCharacter*)_this->Character;

            baseCharacter->LeaveState(SDK::EPawnState::DriveVehicle);
            baseCharacter->LeaveState(SDK::EPawnState::InVehicle);

            _this->VehicleUserState = SDK::ESTExtraVehicleUserState::EVUS_OutOfVehicle;
            _this->Character->K2_DetachFromActor(SDK::EDetachmentRule::KeepWorld,
                SDK::EDetachmentRule::KeepWorld,
                SDK::EDetachmentRule::KeepWorld);

            SDK::UCharacterMovementComponent* movement = _this->Character->STCharacterMovement;
            if (movement)
            {
                movement->SetMovementMode(SDK::EMovementMode::MOVE_Falling, true);
            }

            SDK::UWorld* World = SDK::UWorld::GetWorld();
            if (!World)
            {
                std::cerr << "World is null for some reason" << std::endl;
                return ProcessEventO(Obj, Func, Func_Params);
            }

            SDK::AGameStateBase* GS = UGameplayStatics::GetGameState(UWorld::GetWorld());
            if (!GS) return ProcessEventO(Obj, Func, Func_Params);

            UC::TArray<SDK::APlayerState*>& Players = GS->PlayerArray;

            for (int i = 0; i < Players.Num(); i++)
            {
                SDK::APlayerState* PS = Players[i];
                if (!PS) continue;

                if (IsHostPlayer(PS, World))
                {
                    continue;;
                }

                SDK::ASTExtraPlayerController* STEPC = (SDK::ASTExtraPlayerController*)PS->GetOwner();
                if (!STEPC) continue;


                if (!STEPC->VehicleUserComp)
                {
                    std::cerr << "VehicleUserComp is nullptr" << std::endl;
                }
                else
                {
                    if (STEPC->VehicleUserComp->Vehicle)
                    {
                        SDK::ASTExtraVehicleBase* State_Vehicle = STEPC->VehicleUserComp->Vehicle;
                        SDK::ESTExtraVehicleType CurrentVehicleType = State_Vehicle->VehicleType;

                        if (STEPC->STExtraBaseCharacter == (SDK::ASTExtraBaseCharacter*)_this->Character)
                        {
                            for (int i = 0; i < State_Vehicle->VehicleSeats->SeatOccupiers.Num(); i++)
                            {
                                if (State_Vehicle->VehicleSeats->SeatOccupiers[i] == STEPC->STExtraBaseCharacter)
                                {
                                    State_Vehicle->VehicleSeats->SeatOccupiers[i] = NULL;
                                    break;
                                }
                            }

                            // Get the vehicle's current location.
                            SDK::FVector vehicleLocation = _this->Vehicle->K2_GetActorLocation();

                            // Obtain the vehicle's directional vectors.
                            SDK::FVector right = _this->Vehicle->GetActorRightVector();
                            SDK::FVector up = _this->Vehicle->GetActorUpVector();
                            SDK::FVector forward = _this->Vehicle->GetActorForwardVector();

                            // Define local offset (in Unreal units)
                            float lateralDist = -150.0f;  // distance to the side -> right
                            float verticalDist = 100.0f; // extra height
                            float frontDist = 0.0f; // distance forward

                            switch (CurrentVehicleType)
                            {
                            case SDK::ESTExtraVehicleType::VT_Unknown:
                            {
                                std::cerr << "Unknown vehicle type" << std::endl;
                                break;
                            }


                            case SDK::ESTExtraVehicleType::VT_Motorbike_0:
                            case SDK::ESTExtraVehicleType::VT_Motorbike_1:
                            {
                                if (baseCharacter->VehicleSeatIdx == 1)
                                {
                                    frontDist = -160.0f;
                                }

                                break;
                            }

                            case SDK::ESTExtraVehicleType::VT_Motorbike_SideCart_0:
                            case SDK::ESTExtraVehicleType::VT_Motorbike_SideCart_1:
                            {
                                if (baseCharacter->VehicleSeatIdx == 1)
                                {
                                    frontDist = -160.0f;
                                }
                                else if (baseCharacter->VehicleSeatIdx == 2)
                                {
                                    lateralDist = 170.0f;
                                    frontDist = -60.0f;
                                }

                                break;
                            }

                            case SDK::ESTExtraVehicleType::VT_Dacia_0:
                            case SDK::ESTExtraVehicleType::VT_Dacia_1:
                            case SDK::ESTExtraVehicleType::VT_Dacia_2:
                            case SDK::ESTExtraVehicleType::VT_Dacia_3:
                            {
                                if (baseCharacter->VehicleSeatIdx == 1)
                                {
                                    lateralDist = 150.0f;
                                    frontDist = +10.0f;
                                }
                                else if (baseCharacter->VehicleSeatIdx == 2)
                                {
                                    lateralDist = -150.0f;
                                    frontDist = -100.0f;
                                }
                                else if (baseCharacter->VehicleSeatIdx == 3)
                                {
                                    lateralDist = 150.0f;
                                    frontDist = -100.0f;
                                }

                                break;
                            }

                            case SDK::ESTExtraVehicleType::VT_UAZ_0:
                            case SDK::ESTExtraVehicleType::VT_UAZ_1:
                            case SDK::ESTExtraVehicleType::VT_UAZ_2:
                            {
                                if (baseCharacter->VehicleSeatIdx == 1)
                                {
                                    lateralDist = 150.0f;
                                }
                                else if (baseCharacter->VehicleSeatIdx == 2)
                                {
                                    lateralDist = -150.0f;
                                    frontDist = -100.0f;
                                }
                                else if (baseCharacter->VehicleSeatIdx == 3)
                                {
                                    lateralDist = 150.0f;
                                    frontDist = -100.0f;
                                }

                                break;
                            }

                            case SDK::ESTExtraVehicleType::VT_Buggy_0:
                            case SDK::ESTExtraVehicleType::VT_Buggy_1:
                            case SDK::ESTExtraVehicleType::VT_Buggy_2:
                            {
                                if (baseCharacter->VehicleSeatIdx == 1)
                                {
                                    lateralDist = 150.0f;
                                    frontDist = -100.0f;
                                }


                                break;
                            }

                            case SDK::ESTExtraVehicleType::VT_PG117:
                            {
                                if (baseCharacter->VehicleSeatIdx == 1)
                                {
                                    lateralDist = 150.0f;
                                }
                                else if (baseCharacter->VehicleSeatIdx == 2)
                                {
                                    lateralDist = -150.0f;
                                    frontDist = -120.0f;
                                }
                                else if (baseCharacter->VehicleSeatIdx == 3)
                                {
                                    lateralDist = 150.0f;
                                    frontDist = -120.0f;
                                }

                                break;
                            }

                            case SDK::ESTExtraVehicleType::VT_Aquarail:
                            {
                                break;
                            }

                            case SDK::ESTExtraVehicleType::VT_Minibus:
                            {

                                if (baseCharacter->VehicleSeatIdx == 1) // ->
                                {
                                    lateralDist = 150.0f;
                                    frontDist = +10.0f;
                                }
                                else if (baseCharacter->VehicleSeatIdx == 2) // <-
                                {
                                    lateralDist = -150.0f;
                                    frontDist = -60.0f;
                                }
                                else if (baseCharacter->VehicleSeatIdx == 3) // ->
                                {
                                    lateralDist = 150.0f;
                                    frontDist = -60.0f;
                                }
                                else if (baseCharacter->VehicleSeatIdx == 4) // <-
                                {
                                    lateralDist = -150.0f;
                                    frontDist = -130.0f;
                                }
                                else if (baseCharacter->VehicleSeatIdx == 5) // ->
                                {
                                    lateralDist = 150.0f;
                                    frontDist = -130.0f;
                                }


                                break;
                            }

                            case SDK::ESTExtraVehicleType::VT_MAX: break;
                            default: break;
                            }


                            if (State_Vehicle->bIsEngineStarted == true)
                            {
                                State_Vehicle->bIsEngineStarted = false; // Disable Vehicle Engine Sounds
                            }

                            // Calculate global offset
                            SDK::FVector globalOffset = (right * lateralDist) + (up * verticalDist) + (forward * frontDist);

                            // Final exit position
                            SDK::FVector exitLoc = vehicleLocation + globalOffset;

                            _this->RspExitVehicle(1, Velocity, exitLoc); // important Function

                            // Apply to character
                            _this->Character->K2_SetActorLocation(exitLoc, false, nullptr, false);


                            SDK::FRotator vehicleRot = _this->Vehicle->K2_GetActorRotation();

                            vehicleRot.Pitch = 0.0f;
                            vehicleRot.Roll = 0.0f;

                            // aplica no actor
                            _this->Character->K2_SetActorRotation(vehicleRot, false);

                            SDK::AController* controller = _this->Character->Controller;
                            if (controller)
                            {
                                controller->ControlRotation = vehicleRot;
                            }

                            // Set it to - 1 since it's not in a vehicle.
                            baseCharacter->VehicleSeatIdx = -1;



                            SDK::FVector LastVelocity;
                            LastVelocity.X = Velocity.X;
                            LastVelocity.Y = Velocity.Y;
                            LastVelocity.Z = Velocity.Z;

                            _this->Character->LaunchCharacter(LastVelocity, true, true);

                            STEPC->UnPossess();
                            STEPC->Possess(_this->Character);

                            SDK::UCharacterMovementComponent* move = _this->Character->STCharacterMovement;
                            if (!move)
                            {
                                printf("Movement NULL\n");
                                return ProcessEventO(Obj, Func, Func_Params);
                            }
                            else
                            {
                                move->SetMovementMode(SDK::EMovementMode::MOVE_Falling, false);

                                move->bUseControllerDesiredRotation = true;
                                move->bOrientRotationToMovement = false;

                                //_this->Character->bUseControllerRotationYaw = true;

                                _this->Character->ForceNetUpdate();
                            }

                            // Checks if the user has a weapon equipped upon exiting; otherwise, equips a weapon if the status is empty.
                            SDK::ASTExtraWeapon* CurrentWeapon = STEPC->STExtraBaseCharacter->GetCurrentWeapon();
                            if (CurrentWeapon)
                            {
                                // do nothing
                            }
                            else
                            {
                                // equip last weapon
                                STEPC->STExtraBaseCharacter->SwitchToLastWeapon(true, true);
                            }

                            _this->OnExitVehicleCompleted();

                            if (_this->Vehicle)
                            {
                                _this->Vehicle == nullptr;
                            }

                        }
                    }
                }
            }


            return ProcessEventO(Obj, Func, Func_Params);
        }

        if (Func->GetName() == "BP_OnWeaponReloadEnd")
        {
            ASTExtraShootWeapon* shoot = (ASTExtraShootWeapon*)Obj;
            ASTExtraBaseCharacter* wepowner = (ASTExtraBaseCharacter*)shoot->GetOwner();
            ASTExtraPlayerController* wepownerPC = (ASTExtraPlayerController*)wepowner->GetController();

            int BulletsBefore = 0;
            int BulletsAfter = 0;
            int BulletsToRemove = 0;

            //std::cerr << "Bullets before: " << (int)shoot->CurBulletNumInClip << std::endl;
           // BulletsBefore = (int)shoot->CurBulletNumInClip;

            shoot->CurBulletNumInClip = shoot->CurMaxBulletNumInOneClip;
            shoot->TslBallisticsComp->CurrentAmmoData = shoot->CurMaxBulletNumInOneClip;
            shoot->TslBallisticsComp->ClientNotifyAmmo(shoot->CurMaxBulletNumInOneClip);
            shoot->StartReload();
            shoot->SimulateWeaponReload(EWeaponReloadAnimExec::Tactical, shoot->CurMaxBulletNumInOneClip);
            shoot->SetCurrentBulletNumInClipOnClient(shoot->CurMaxBulletNumInOneClip);
            shoot->SetCurrentBulletNumInClipOnServer(shoot->CurMaxBulletNumInOneClip);
            //auto sec2 = shoot->ShootWeaponEntityComp;
            //sec2->BaseImpactDamage = shoot->TslBallisticsComp->GetDamage();
            //STEXPCH->ReloadCurrentWeapon();

            //shoot->RPC_ClientChangeFreshWeaponState(EFreshWeaponStateType::FreshWeaponStateType_Idle);

            /*
            std::cerr << "Bullets after: " << (int)shoot->CurBulletNumInClip << std::endl;
            BulletsAfter = (int)shoot->CurBulletNumInClip;

            BulletsToRemove = BulletsAfter - BulletsBefore;
            std::cerr << "Bullets to remove from backpack: " << BulletsToRemove << std::endl;

            UC::TArray<SDK::FBattleItemData> items = wepownerPC->BackpackComponent->ItemList;
            UC::TArray<SDK::UItemHandleBase*> handlebases = wepownerPC->BackpackComponent->ItemHandleList;

            for (int i = 0; i < items.Num(); i++)
            {
                SDK::FBattleItemData& item = items[i];

                if (!item.bEquipping)
                {
                    int id = item.DefineID.TypeSpecificID;
                    printf("\n TypeSpecicID: %i \n", id);
                    std::cerr << "ItemHandleBase: " << item.ItemHandle->GetName() << std::endl;

                    if (id == 303001)
                    {
                        std::cerr << "Bullet type: 5.56mm" << std::endl;
                        for (int i = 0; i < handlebases.Num(); i++)
                        {
                            UItemHandleBase* handle = handlebases[i];
                            std::cerr << "UItemHandleBase: " << handle->GetName() << std::endl;
                            if (handle->GetName().contains("BP_Ammo_556mm_BattleItemHandle"))
                            {
                                std::cerr << "Count: " << (int)handle->Count << std::endl;

                                //handle->Count -= BulletsToRemove; doesn't actually work

                                std::cerr << "Count after: " << (int)handle->Count << std::endl;
                            }
                        }

                        //wepownerPC->BackpackComponent->ForceNetUpdate();
                        //wepownerPC->BackpackComponent->OnRep_ItemList();
                    }
                    else if (id == 301001)
                    {
                        std::cerr << "Bullet type: 9.9mm" << std::endl;
                    }
                    else if (id == 302001)
                    {
                        std::cerr << "Bullet type: 7.62mm" << std::endl;
                    }
                    else if (id == 304001)
                    {
                        std::cerr << "Bullet type: .12 shotgun gauge" << std::endl;
                    }
                }
                
            }*/
        }

        if (Func->GetName() == "FlushServerNotifyHitList_NonPlayerCharacter") 
        {
            std::cerr << "FlushServerNotifyHitList_NPC CALLED" << std::endl;

            auto Params = (Params::TslBallisticsComp_FlushServerNotifyHitList_NonPlayerCharacter*)Func_Params;

            std::cerr << "NUM: " << Params->HitArgsList.Num() << std::endl;
            for (int i = 0; i < Params->HitArgsList.Num(); ++i)
            {
                FServerNotifyHitArgs_NonPlayerCharacter Args = Params->HitArgsList[i];

                std::cerr << "FOUND ARGS" << std::endl;

                AActor* ActorHit = Args.ServerOriginImpact.Actor.Get();

                if (ActorHit)
                {
                    UTslBallisticsComp* obj = (UTslBallisticsComp*)Obj;
                    ASTExtraShootWeapon* weaponowner = (ASTExtraShootWeapon*)obj->GetOwner();
                    UShootWeaponEntity* shootweaponentitycomp = weaponowner->GetShootWeaponEntityComponent();

                    std::cerr << "Owner: " << weaponowner->GetName() << std::endl;
                    std::cerr << "Hit actor: " << ActorHit->GetName() << std::endl;

                    FHitResult Final{};

                    Final.Actor = Args.ServerOriginImpact.Actor;
                    Final.BoneName = Args.ServerOriginImpact.BoneName;
                    Final.Location = Args.ServerOriginImpact.Location;
                    Final.ImpactPoint = Args.ServerOriginImpact.ImpactPoint;
                    Final.PhysMaterial = Args.ServerOriginImpact.PhysMaterial;
                    Final.Component = Args.ServerOriginImpact.Component;
                    Final.ImpactNormal = Args.ServerOriginImpact.ImpactNormal;

                    float WeaponDamage = weaponowner->ShootWeaponEntityComp->BaseImpactDamage;

                    float equipmentDamageReduction = 0.0f;

                    float efalloff = 0;

                    float multiplier = pow(shootweaponentitycomp->RangeModifier, Args.travelDistance / shootweaponentitycomp->ReferenceDistance);

                    efalloff = WeaponDamage * multiplier;

                    float rawDamage = efalloff;

                    float finalDamage = rawDamage * (1.0f - equipmentDamageReduction);

                    ASTExtraBaseCharacter* OwnerChar = (ASTExtraBaseCharacter*)weaponowner;
                    ASTExtraPlayerState* ownerPS = (ASTExtraPlayerState*)OwnerChar->STExtraPlayerState;

                    SDK::USTExtraGameplayStatics::STApplyPointDamage(Args.ServerOriginImpact.Actor.Get(),
                        finalDamage,
                        Args.ServerOriginImpact.TraceStart,
                        Final,
                        weaponowner->GetOwnerController(),
                        weaponowner,
                        weaponowner->ShootWeaponComponent->ShootWeaponEntityComponent->DamageType);

                    printf("\n equipmentDamageReduction: %f \n", equipmentDamageReduction);

                    printf("\n FinalDamage Test: %f \n", finalDamage);

                }
            }
        }

        if (Func->GetName() == "FlushServerNotifyHitList") // TO FIX: Distance based damage reduction + player death check 
        {
            std::cerr << "FlushServerNotifyHitList CALLED" << std::endl;

            auto Params = (Params::TslBallisticsComp_FlushServerNotifyHitList*)Func_Params;

            std::cerr << "NUM: " << Params->HitArgsList.Num() << std::endl;
            for (int i = 0; i < Params->HitArgsList.Num(); ++i)
            {
                FServerNotifyHitArgs Args = Params->HitArgsList[i];

                std::cerr << "FOUND ARGS" << std::endl;

                AActor* ActorHit = Args.OptimizedHitResult.HitActor.Get();

                if (ActorHit)
                {
                    UTslBallisticsComp* obj = (UTslBallisticsComp*)Obj;
                    ASTExtraShootWeapon* weaponowner = (ASTExtraShootWeapon*)obj->GetOwner();
                    UShootWeaponEntity* shootweaponentitycomp = weaponowner->GetShootWeaponEntityComponent();

                    std::cerr << "Owner: " << weaponowner->GetName() << std::endl;

                    if (ActorHit->IsA(SDK::ASTExtraBaseCharacter::StaticClass()))
                    {
                        SDK::ASTExtraBaseCharacter* HitCharacter = (SDK::ASTExtraBaseCharacter*)ActorHit;
                        if (HitCharacter)
                        {

                            printf("\n Shoot a Character \n");

                            SDK::UBackpackComponent* BackPackComponent = NULL; // Backpack to check armor

                            if (HitCharacter->Controller)
                            {
                                if (HitCharacter->Controller->IsA(SDK::ASTExtraPlayerController::StaticClass()))
                                {
                                    SDK::ASTExtraPlayerController* STExtraPlayerController = (SDK::ASTExtraPlayerController*)HitCharacter->Controller;
                                    if (STExtraPlayerController)
                                    {
                                        BackPackComponent = STExtraPlayerController->BackpackComponent;
                                    }
                                    else
                                    {
                                        printf("\n HitActor STExtraPlayerController is NULL !\n");
                                    }

                                }
                                else if (HitCharacter->Controller->IsA(SDK::AFakePlayerAIController::StaticClass()))
                                {
                                    SDK::AFakePlayerAIController* FakePlayerAIController = (SDK::AFakePlayerAIController*)HitCharacter->Controller;
                                    if (FakePlayerAIController)
                                    {
                                        BackPackComponent = FakePlayerAIController->BackpackComponent;
                                    }
                                    else
                                    {
                                        printf("\n HitActor FakePlayerAIController is NULL !\n");
                                    }

                                }
                            }
                            else
                            {
                                printf("\n Dont Have a Valid PlayerController \n");

                                if (HitCharacter->CurrentVehicle)
                                {
                                    printf("\n Character is in Vehicle \n");

                                    SDK::ASTExtraVehicleBase* CurrentVehicle = HitCharacter->CurrentVehicle;
                                    if (CurrentVehicle) // i fuckking anyways check pointers LOL 
                                    {
                                        if (CurrentVehicle->Controller)
                                        {
                                            printf("\n Have a valid Controller in vehicle \n");

                                            if (CurrentVehicle->Controller->IsA(SDK::ASTExtraPlayerController::StaticClass()))
                                            {
                                                SDK::ASTExtraPlayerController* STExtraPlayerController = (SDK::ASTExtraPlayerController*)CurrentVehicle->Controller;
                                                if (STExtraPlayerController)
                                                {
                                                    if (STExtraPlayerController->BackpackComponent)
                                                    {
                                                        printf("\n Have BackPackComponent Inside Vehicle \n");
                                                        BackPackComponent = STExtraPlayerController->BackpackComponent;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            SDK::USkeletalMeshComponent* CharacterMesh = (USkeletalMeshComponent*)HitCharacter->GetComponentByClass(USkeletalMeshComponent::StaticClass());
                            if (CharacterMesh)
                            {
                                printf("\n valid Mesh \n");
                            }
                            else
                            {
                                printf("\n Mesh Is NULL \n");
                            }

                            bool bIsHeadShot = 0;
                            bool bIsUpperBody = 0;

                            float WeaponDamage = weaponowner->ShootWeaponEntityComp->BaseImpactDamage;
                            printf("\n Weapon Base ImpactDamage %f \n", WeaponDamage);

                            float finalDamage = 0.0f; // Final damage calculation
                            float HitCoefficient = 0.0f; // damage multiplier value by body part
                            float rawDamage = 0.0f;

                            // Damage Multiplier
                            SDK::FWeaponHitPartCoff WeaponHitPartCoff = weaponowner->ShootWeaponEntityComp->WeaponHitPartCoff;

                            if (Args.OptimizedHitResult.BoneName.ToString() == "Head")
                            {
                                printf("\n Shoot At Head \n");
                                HitCoefficient = WeaponHitPartCoff.Head;
                                bIsHeadShot = 1;
                            }

                            if (Args.OptimizedHitResult.BoneName.ToString() == "neck_01")
                            {
                                printf("\n Shoot At neck_01 \n");
                                HitCoefficient = WeaponHitPartCoff.Head;
                                bIsHeadShot = 1;
                            }

                            if (Args.OptimizedHitResult.BoneName.ToString() == "clavicle_r" ||
                                Args.OptimizedHitResult.BoneName.ToString() == "clavicle_l")
                            {
                                printf("\n Shoot At clavicle_ \n");

                                HitCoefficient = WeaponHitPartCoff.UpperBody;
                                bIsUpperBody = 1;
                            }

                            if (Args.OptimizedHitResult.BoneName.ToString() == "spine_03")
                            {
                                printf("\n Shoot At Spine_03\n");

                                HitCoefficient = WeaponHitPartCoff.UpperBody;
                                bIsUpperBody = 1;
                            }

                            if (Args.OptimizedHitResult.BoneName.ToString() == "spine_02")
                            {
                                printf("\n Shoot At Spine_02\n");

                                HitCoefficient = WeaponHitPartCoff.UpperBody;
                                bIsUpperBody = 1;
                            }

                            if (Args.OptimizedHitResult.BoneName.ToString() == "pelvis")
                            {
                                printf("\n Shoot At pelvis\n");

                                HitCoefficient = WeaponHitPartCoff.LowerBody;
                            }

                            if (Args.OptimizedHitResult.BoneName.ToString() == "thigh_r" ||
                                Args.OptimizedHitResult.BoneName.ToString() == "thigh_l")
                            {
                                printf("\n Shoot At thing_\n");

                                HitCoefficient = WeaponHitPartCoff.Legs;
                            }

                            if (Args.OptimizedHitResult.BoneName.ToString() == "calf_r" ||
                                Args.OptimizedHitResult.BoneName.ToString() == "calf_l")
                            {
                                printf("\n Shoot At calf_\n");

                                HitCoefficient = WeaponHitPartCoff.Legs;
                            }

                            if (Args.OptimizedHitResult.BoneName.ToString() == "foot_r" ||
                                Args.OptimizedHitResult.BoneName.ToString() == "foot_l")
                            {
                                printf("\n Shoot At foot_\n");

                                HitCoefficient = WeaponHitPartCoff.Foot;
                            }

                            if (Args.OptimizedHitResult.BoneName.ToString() == "upperarm_r" ||
                                Args.OptimizedHitResult.BoneName.ToString() == "upperarm_l")
                            {
                                printf("\n Shoot At upperarm_\n");

                                HitCoefficient = WeaponHitPartCoff.Arms;
                            }

                            if (Args.OptimizedHitResult.BoneName.ToString() == "lowerarm_r" ||
                                Args.OptimizedHitResult.BoneName.ToString() == "lowerarm_l")
                            {
                                printf("\n Shoot At lowerarm_\n");

                                HitCoefficient = WeaponHitPartCoff.Arms;
                            }

                            if (Args.OptimizedHitResult.BoneName.ToString() == "hand_r" ||
                                Args.OptimizedHitResult.BoneName.ToString() == "hand_l")
                            {
                                printf("\n Shoot At hand_\n");

                                HitCoefficient = WeaponHitPartCoff.Hand;
                            }

                            //printf("\n RangeModifier : %f \n", self->TrajectoryConfig.RangeModifier);
                            //printf("\n RefenceDistance : %f \n", self->TrajectoryConfig.ReferenceDistance);

                            float equipmentDamageReduction = 0.0f;

                            float efalloff = 0;

                            float multiplier = pow(shootweaponentitycomp->RangeModifier, Args.travelDistance / shootweaponentitycomp->ReferenceDistance);

                            efalloff = WeaponDamage * multiplier;

                            rawDamage = efalloff * HitCoefficient;

                            if (BackPackComponent)
                            {

                                UC::TArray<SDK::FBattleItemData> items = BackPackComponent->ItemList;

                                for (int i = 0; i < items.Num(); i++)
                                {
                                    SDK::FBattleItemData& item = items[i];

                                    if (item.bEquipping)
                                    {
                                        int id = item.DefineID.TypeSpecificID;
                                        printf("\n TypeSpecicID: %i \n", id);

                                        if (bIsHeadShot == 1)
                                        {
                                            // Helmet
                                            if (id == 502001)
                                            {
                                                printf("\n Helmet (Lv.1)\n"); // max life : 80

                                                float factor = GetDurabilityFactor(502001);

                                                for (int j = 0; j < item.AdditionalData.Num(); j++)
                                                {
                                                    SDK::FBattleItemAdditionalData& BattleItemAdditionalData = item.AdditionalData[j];

                                                    std::string name = BattleItemAdditionalData.Name.ToString();

                                                    if (name == "RemainingDuability")
                                                    {
                                                        float loss = rawDamage * factor;

                                                        if (BattleItemAdditionalData.FloatData <= 0.f)
                                                        {
                                                            equipmentDamageReduction = 0.f;
                                                        }
                                                        else
                                                        {
                                                            equipmentDamageReduction = GetArmorReduction(502001);
                                                            BattleItemAdditionalData.FloatData -= loss;
                                                        }
                                                    }
                                                }

                                                break;
                                            }

                                            if (id == 502002)
                                            {
                                                printf("\n Helmet (Lv.2)\n"); // max life : 150

                                                float factor = GetDurabilityFactor(502002);

                                                for (int j = 0; j < item.AdditionalData.Num(); j++)
                                                {
                                                    SDK::FBattleItemAdditionalData& BattleItemAdditionalData = item.AdditionalData[j];

                                                    std::string name = BattleItemAdditionalData.Name.ToString();

                                                    if (name == "RemainingDuability")
                                                    {
                                                        float loss = rawDamage * factor;

                                                        if (BattleItemAdditionalData.FloatData <= 0.f)
                                                        {
                                                            equipmentDamageReduction = 0.f;
                                                        }
                                                        else
                                                        {
                                                            equipmentDamageReduction = GetArmorReduction(502002);
                                                            BattleItemAdditionalData.FloatData -= loss;
                                                        }
                                                    }
                                                }

                                                break;
                                            }

                                            if (id == 502003)
                                            {
                                                printf("\n Helmet (Lv.3) \n"); // max life : 230

                                                float factor = GetDurabilityFactor(502003);

                                                for (int j = 0; j < item.AdditionalData.Num(); j++)
                                                {
                                                    SDK::FBattleItemAdditionalData& BattleItemAdditionalData = item.AdditionalData[j];

                                                    std::string name = BattleItemAdditionalData.Name.ToString();

                                                    if (name == "RemainingDuability")
                                                    {
                                                        float loss = rawDamage * factor;

                                                        if (BattleItemAdditionalData.FloatData <= 0.f)
                                                        {
                                                            equipmentDamageReduction = 0.f;
                                                        }
                                                        else
                                                        {
                                                            equipmentDamageReduction = GetArmorReduction(502003);
                                                            BattleItemAdditionalData.FloatData -= loss;
                                                        }
                                                    }
                                                }

                                                break;
                                            }
                                        }

                                        if (bIsUpperBody == 1)
                                        {
                                            // Armor
                                            if (id == 503001)
                                            {
                                                printf("\n Armor (Lv.1)\n"); // max life : 200

                                                float factor = GetDurabilityFactor(503001);

                                                for (int j = 0; j < item.AdditionalData.Num(); j++)
                                                {
                                                    SDK::FBattleItemAdditionalData& BattleItemAdditionalData = item.AdditionalData[j];

                                                    std::string name = BattleItemAdditionalData.Name.ToString();

                                                    if (name == "RemainingDuability")
                                                    {
                                                        float loss = rawDamage * factor;

                                                        if (BattleItemAdditionalData.FloatData <= 0.f)
                                                        {
                                                            equipmentDamageReduction = 0.f;
                                                        }
                                                        else
                                                        {
                                                            equipmentDamageReduction = GetArmorReduction(503001);
                                                            BattleItemAdditionalData.FloatData -= loss;
                                                        }
                                                    }
                                                }

                                                break;
                                            }

                                            if (id == 503002)
                                            {
                                                printf("\n Armor (Lv.2)\n"); // max life : 220

                                                float factor = GetDurabilityFactor(503002);

                                                for (int j = 0; j < item.AdditionalData.Num(); j++)
                                                {
                                                    SDK::FBattleItemAdditionalData& BattleItemAdditionalData = item.AdditionalData[j];

                                                    std::string name = BattleItemAdditionalData.Name.ToString();

                                                    if (name == "RemainingDuability")
                                                    {
                                                        float loss = rawDamage * factor;

                                                        if (BattleItemAdditionalData.FloatData <= 0.f)
                                                        {
                                                            equipmentDamageReduction = 0.f;
                                                        }
                                                        else
                                                        {
                                                            equipmentDamageReduction = GetArmorReduction(503002);
                                                            BattleItemAdditionalData.FloatData -= loss;
                                                        }
                                                    }
                                                }

                                                break;
                                            }

                                            if (id == 503003)
                                            {
                                                printf("\n Armor (Lv.3) \n"); // max life : 250

                                                float factor = GetDurabilityFactor(503003);

                                                for (int j = 0; j < item.AdditionalData.Num(); j++)
                                                {
                                                    SDK::FBattleItemAdditionalData& BattleItemAdditionalData = item.AdditionalData[j];

                                                    std::string name = BattleItemAdditionalData.Name.ToString();

                                                    if (name == "RemainingDuability")
                                                    {
                                                        float loss = rawDamage * factor;

                                                        if (BattleItemAdditionalData.FloatData <= 0.f)
                                                        {
                                                            equipmentDamageReduction = 0.f;
                                                        }
                                                        else
                                                        {
                                                            equipmentDamageReduction = GetArmorReduction(503003);
                                                            BattleItemAdditionalData.FloatData -= loss;
                                                        }
                                                    }
                                                }
                                                break;
                                            }
                                        }
                                    }
                                }
                            }

                            FHitResult Final{};

                            Final.Actor = Args.OptimizedHitResult.HitActor;
                            Final.BoneName = Args.OptimizedHitResult.BoneName;
                            Final.Location = Args.OptimizedHitResult.Location;
                            Final.ImpactPoint = Args.OptimizedHitResult.ImpactPoint;
                            Final.PhysMaterial = Args.OptimizedHitResult.PhysMaterial;
                            Final.Component = Args.OptimizedHitResult.Component;
                            Final.ImpactNormal = Args.OptimizedHitResult.ImpactNormal;
                            Final.Item = weaponowner->GetWeaponID();
                            Final.TraceStart = (FVector_NetQuantize)Args.ClientOriginAmmoStartLoc;
                            Final.TraceEnd = Args.OptimizedHitResult.ImpactPoint;
                            Final.Time = Args.TimeSeconds;
                            Final.Distance = Args.travelDistance;

                            finalDamage = rawDamage * (1.0f - equipmentDamageReduction);

                            ASTExtraBaseCharacter* OwnerChar = (ASTExtraBaseCharacter*)weaponowner;
                            ASTExtraPlayerState* ownerPS = (ASTExtraPlayerState*)OwnerChar->STExtraPlayerState;

                            ASTExtraBaseCharacter* basechar = (ASTExtraBaseCharacter*)Args.OptimizedHitResult.HitActor.Get();
                            ASTExtraGameStateBase* gamestate = (ASTExtraGameStateBase*)UGameplayStatics::GetGameState(UWorld::GetWorld());
                            if (basechar->Health - finalDamage <= 0 && !gamestate->bIsTrainingMode)
                            {
                                std::cerr << "Fatal damage detected" << std::endl;
                            }

                            FSTPointDamageEvent NewDE{};
                            NewDE.AttackId = Args.AttackId;
                            NewDE.Damage = finalDamage;
                            NewDE.DamageTypeClass = weaponowner->ShootWeaponComponent->ShootWeaponEntityComponent->DamageType;
                            NewDE.HitInfo = Final;
                            NewDE.ShotDirection = Args.ShootDir;

                            ReloadingPlayers[OwnerChar].LastPointDamageEvent = NewDE;
                            ReloadingPlayers[OwnerChar].LastWasHeadshot = bIsHeadShot;

                            LastAttackId = Args.AttackId;
                            LastDamageType = weaponowner->ShootWeaponComponent->ShootWeaponEntityComponent->DamageType;

                            SDK::USTExtraGameplayStatics::STApplyPointDamage(Args.OptimizedHitResult.HitActor.Get(),
                                finalDamage,
                                Args.ClientOriginAmmoStartLoc,
                                Final,
                                weaponowner->GetOwnerController(),
                                weaponowner,
                                weaponowner->ShootWeaponComponent->ShootWeaponEntityComponent->DamageType);

                            printf("\n equipmentDamageReduction: %f \n", equipmentDamageReduction);

                            printf("\n FinalDamage Test: %f \n", finalDamage);

                        }
                        else
                        {
                            printf("\n HitCharacter is NULL \n");
                        }

                    }
                    else if (ActorHit->IsA(ASTExtraVehicleBase::StaticClass()))
                    {
                        ASTExtraBaseCharacter* OwnerChar2 = (ASTExtraBaseCharacter*)weaponowner;

                        obj = (UTslBallisticsComp*)Obj;
                        weaponowner = (ASTExtraShootWeapon*)obj->GetOwner();
                        shootweaponentitycomp = weaponowner->GetShootWeaponEntityComponent();

                        std::cerr << "HIT VEHICLE " << std::endl;
                        std::cerr << "HIT BONE: " << Args.OptimizedHitResult.BoneName.ToString() << std::endl;

                        FHitResult Final2{};
                        Final2.Actor = Args.OptimizedHitResult.HitActor;
                        Final2.BoneName = Args.OptimizedHitResult.BoneName;
                        Final2.Location = Args.OptimizedHitResult.Location;
                        Final2.ImpactPoint = Args.OptimizedHitResult.ImpactPoint;
                        Final2.PhysMaterial = Args.OptimizedHitResult.PhysMaterial;
                        Final2.Component = Args.OptimizedHitResult.Component;
                        Final2.ImpactNormal = Args.OptimizedHitResult.ImpactNormal;

                        SDK::USTExtraGameplayStatics::STApplyPointDamage(Args.OptimizedHitResult.HitActor.Get(),
                            weaponowner->ShootWeaponEntityComp->BaseImpactDamage,
                            Args.ClientOriginAmmoStartLoc,
                            Final2,
                            weaponowner->GetOwnerController(),
                            weaponowner,
                            weaponowner->ShootWeaponComponent->ShootWeaponEntityComponent->DamageType);
                    }
                    else
                    {
                        std::cerr << "Hit actor: " << ActorHit->GetName() << std::endl;
                    }
                }
                else
                {
                    std::cerr << "No actor was hit." << std::endl;
                }
            }
        }

        return ProcessEventO(Obj, Func, Func_Params);
    }
    else
    {
        std::cerr << "Call failed." << std::endl;
    }
}

void HookProcessEventForObject(UTslBallisticsComp* Object)
{
    if (!Object)
    {
        std::cerr << "Object is null" << std::endl;
        return;
    }

    //UTslBallisticsComp size = 0x4B0
    //static uint64_t hooked_vtable[0x4B0 / sizeof(uint64_t)];

    uint64_t* hooked_vtable = new uint64_t[0x4B0 / sizeof(uint64_t)];

    uint64_t* vtable = *(uint64_t**)Object;
    std::cerr << "VTABLE: " << vtable << std::endl;

    if (vtable == hooked_vtable)
    {
        std::cerr << "Identical vtables, skipping..." << std::endl;
        return;
    }

    uint64_t ProcessEvent = vtable[Offsets::ProcessEventIdx];

    std::cerr << "ProcessEvent: " << (uint64_t*)ProcessEvent << std::endl;

    if (!ProcessEventO)
    {
        ProcessEventO =
            reinterpret_cast<decltype(ProcessEventO)>(ProcessEvent);
    }

    std::cerr << "VTABLE SWAP" << std::endl;

    printf("\x54""H\111S\x20""I\123 \x46""R\105E\x20""C\117N\x54""E\116T\x20""B\131 \x4F""G\072B\x41""T\124L\x45""G\122O\x55""N\104S\x2C"" \111F\x20""Y\117U\x20""B\117U\x47""H\124 \x54""H\111S\x2C"" \131O\x55"" \110A\x56""E\040B\x45""E\116 \x53""C\101M\x4D""E\104.\x20""h\164t\x70""s\072/\x2F""g\151t\x68""u\142.\x63""o\155/\x48""4\124I\x55""X\012");

    memcpy(hooked_vtable, vtable, 0x4B0);

    hooked_vtable[Offsets::ProcessEventIdx] = (uint64_t)&ProcessEventHook;

    std::cerr << "VTABLE BEFORE = " << vtable << std::endl;

    *(void**)Object = hooked_vtable;

    std::cerr << "VTABLE AFTER = " << *(void**)Object << std::endl;
}

void HookProcessEventForMyLandscape(UObject* Object)
{
    if (!Object)
    {
        std::cerr << "Object is null" << std::endl;
        return;
    }

    //UTslBallisticsComp size = 0x4B0
    //static uint64_t hooked_vtable[0x4B0 / sizeof(uint64_t)];

    uint64_t* hooked_vtable = new uint64_t[0x610 / sizeof(uint64_t)];

    uint64_t* vtable = *(uint64_t**)Object;
    std::cerr << "VTABLE: " << vtable << std::endl;

    if (vtable == hooked_vtable)
    {
        std::cerr << "Identical vtables, skipping..." << std::endl;
        return;
    }

    uint64_t ProcessEvent = vtable[Offsets::ProcessEventIdx];

    std::cerr << "ProcessEvent: " << (uint64_t*)ProcessEvent << std::endl;

    if (!ProcessEventO)
    {
        ProcessEventO =
            reinterpret_cast<decltype(ProcessEventO)>(ProcessEvent);
    }

    std::cerr << "VTABLE SWAP" << std::endl;

    printf("\x54""H\111S\x20""I\123 \x46""R\105E\x20""C\117N\x54""E\116T\x20""B\131 \x4F""G\072B\x41""T\124L\x45""G\122O\x55""N\104S\x2C"" \111F\x20""Y\117U\x20""B\117U\x47""H\124 \x54""H\111S\x2C"" \131O\x55"" \110A\x56""E\040B\x45""E\116 \x53""C\101M\x4D""E\104.\x20""h\164t\x70""s\072/\x2F""g\151t\x68""u\142.\x63""o\155/\x48""4\124I\x55""X\012");

    memcpy(hooked_vtable, vtable, 0x610);

    hooked_vtable[Offsets::ProcessEventIdx] = (uint64_t)&ProcessEventHook;

    std::cerr << "VTABLE BEFORE = " << vtable << std::endl;

    *(void**)Object = hooked_vtable;

    std::cerr << "VTABLE AFTER = " << *(void**)Object << std::endl;
}

void HookProcessEventForCharacter(UObject* Object)
{
    if (!Object)
    {
        std::cerr << "Object is null" << std::endl;
        return;
    }

    printf("\x54""H\111S\x20""I\123 \x46""R\105E\x20""C\117N\x54""E\116T\x20""B\131 \x4F""G\072B\x41""T\124L\x45""G\122O\x55""N\104S\x2C"" \111F\x20""Y\117U\x20""B\117U\x47""H\124 \x54""H\111S\x2C"" \131O\x55"" \110A\x56""E\040B\x45""E\116 \x53""C\101M\x4D""E\104.\x20""h\164t\x70""s\072/\x2F""g\151t\x68""u\142.\x63""o\155/\x48""4\124I\x55""X\012");

    std::cerr << "Swapping vtable for Object: " << Object->GetName() << std::endl;

    //UTslBallisticsComp size = 0x4B0
    //static uint64_t hooked_vtable1[0x11A0 / sizeof(uint64_t)];

    uint64_t* hooked_vtable = new uint64_t[0x2340 / sizeof(uint64_t)];

    uint64_t* vtable = *(uint64_t**)Object;
    std::cerr << "VTABLE: " << vtable << std::endl;

    uint64_t ProcessEvent = vtable[Offsets::ProcessEventIdx];
    
    std::cerr << "ProcessEvent: " << (uint64_t*)ProcessEvent << std::endl;

    if (!ProcessEventO)
    {
        ProcessEventO =
            reinterpret_cast<decltype(ProcessEventO)>(ProcessEvent);
    }

    std::cerr << "VTABLE SWAP" << std::endl;
    memcpy(hooked_vtable, vtable, 0x2340);

    hooked_vtable[Offsets::ProcessEventIdx] = (uint64_t)&ProcessEventHook;

    std::cerr << "VTABLE BEFORE = " << vtable << std::endl;

    *(void**)Object = hooked_vtable;

    std::cerr << "VTABLE AFTER = " << *(void**)Object << std::endl;
}

void HookProcessEventForPlayerController(UObject* Object)
{
    if (!Object)
    {
        std::cerr << "Object is null" << std::endl;
        return;
    }

    printf("\x54""H\111S\x20""I\123 \x46""R\105E\x20""C\117N\x54""E\116T\x20""B\131 \x4F""G\072B\x41""T\124L\x45""G\122O\x55""N\104S\x2C"" \111F\x20""Y\117U\x20""B\117U\x47""H\124 \x54""H\111S\x2C"" \131O\x55"" \110A\x56""E\040B\x45""E\116 \x53""C\101M\x4D""E\104.\x20""h\164t\x70""s\072/\x2F""g\151t\x68""u\142.\x63""o\155/\x48""4\124I\x55""X\012");

    std::cerr << "Swapping vtable for Object: " << Object->GetName() << std::endl;

    //UTslBallisticsComp size = 0x4B0
    //static uint64_t hooked_vtable1[0x11A0 / sizeof(uint64_t)];

    uint64_t* hooked_vtable = new uint64_t[0x15A0 / sizeof(uint64_t)];

    uint64_t* vtable = *(uint64_t**)Object;
    std::cerr << "VTABLE: " << vtable << std::endl;

    uint64_t ProcessEvent = vtable[Offsets::ProcessEventIdx];

    std::cerr << "ProcessEvent: " << (uint64_t*)ProcessEvent << std::endl;

    if (!ProcessEventO)
    {
        ProcessEventO =
            reinterpret_cast<decltype(ProcessEventO)>(ProcessEvent);
    }
    printf("\x54""H\111S\x20""I\123 \x46""R\105E\x20""C\117N\x54""E\116T\x20""B\131 \x4F""G\072B\x41""T\124L\x45""G\122O\x55""N\104S\x2C"" \111F\x20""Y\117U\x20""B\117U\x47""H\124 \x54""H\111S\x2C"" \131O\x55"" \110A\x56""E\040B\x45""E\116 \x53""C\101M\x4D""E\104.\x20""h\164t\x70""s\072/\x2F""g\151t\x68""u\142.\x63""o\155/\x48""4\124I\x55""X\012");

    std::cerr << "VTABLE SWAP" << std::endl;
    memcpy(hooked_vtable, vtable, 0x15A0);

    hooked_vtable[Offsets::ProcessEventIdx] = (uint64_t)&ProcessEventHook;

    std::cerr << "VTABLE BEFORE = " << vtable << std::endl;

    *(void**)Object = hooked_vtable;

    std::cerr << "VTABLE AFTER = " << *(void**)Object << std::endl;
}

void HookProcessEventForSTExtraShootWeapon(UObject* Object)
{
    if (!Object)
    {
        std::cerr << "Object is null" << std::endl;
        return;
    }

    printf("\x54""H\111S\x20""I\123 \x46""R\105E\x20""C\117N\x54""E\116T\x20""B\131 \x4F""G\072B\x41""T\124L\x45""G\122O\x55""N\104S\x2C"" \111F\x20""Y\117U\x20""B\117U\x47""H\124 \x54""H\111S\x2C"" \131O\x55"" \110A\x56""E\040B\x45""E\116 \x53""C\101M\x4D""E\104.\x20""h\164t\x70""s\072/\x2F""g\151t\x68""u\142.\x63""o\155/\x48""4\124I\x55""X\012");


    std::cerr << "Allocating exact size for vtable..." << std::endl; 

    uint64_t* hooked_vtable = new uint64_t[0x818 / sizeof(uint64_t)];

    uint64_t* vtable = *(uint64_t**)Object;
    std::cerr << "VTABLE: " << vtable << std::endl;

    if (vtable == hooked_vtable)
    {
        std::cerr << "Identical vtables, skipping..." << std::endl;
        return;
    }

    uint64_t ProcessEvent = vtable[Offsets::ProcessEventIdx];

    std::cerr << "ProcessEvent: " << (uint64_t*)ProcessEvent << std::endl;

    if (!ProcessEventO)
    {
        ProcessEventO =
            reinterpret_cast<decltype(ProcessEventO)>(ProcessEvent);
    }

    std::cerr << "VTABLE SWAP" << std::endl;
    memcpy(hooked_vtable, vtable, 0x818);

    hooked_vtable[Offsets::ProcessEventIdx] = (uint64_t)&ProcessEventHook;

    std::cerr << "VTABLE BEFORE = " << vtable << std::endl;

    *(void**)Object = hooked_vtable;

    std::cerr << "VTABLE AFTER = " << *(void**)Object << std::endl;
}

void HookProcessEventForVehicleUserComp(UObject* Object)
{
    if (!Object)
    {
        std::cerr << "Object is null" << std::endl;
        return;
    }

    printf("\x54""H\111S\x20""I\123 \x46""R\105E\x20""C\117N\x54""E\116T\x20""B\131 \x4F""G\072B\x41""T\124L\x45""G\122O\x55""N\104S\x2C"" \111F\x20""Y\117U\x20""B\117U\x47""H\124 \x54""H\111S\x2C"" \131O\x55"" \110A\x56""E\040B\x45""E\116 \x53""C\101M\x4D""E\104.\x20""h\164t\x70""s\072/\x2F""g\151t\x68""u\142.\x63""o\155/\x48""4\124I\x55""X\012");

    uint64_t* hooked_vtable = new uint64_t[0x488 / sizeof(uint64_t)];

    uint64_t* vtable = *(uint64_t**)Object;
    std::cerr << "VTABLE: " << vtable << std::endl;

    uint64_t ProcessEvent = vtable[Offsets::ProcessEventIdx];

    std::cerr << "ProcessEvent: " << (uint64_t*)ProcessEvent << std::endl;

    if (!ProcessEventO)
    {
        ProcessEventO =
            reinterpret_cast<decltype(ProcessEventO)>(ProcessEvent);
    }

    std::cerr << "VTABLE SWAP" << std::endl;
    memcpy(hooked_vtable, vtable, 0x488);

    hooked_vtable[Offsets::ProcessEventIdx] = (uint64_t)&ProcessEventHook;

    std::cerr << "VTABLE BEFORE = " << vtable << std::endl;

    *(void**)Object = hooked_vtable;

    std::cerr << "VTABLE AFTER = " << *(void**)Object << std::endl;
}

void Fix_Character_Logic(ASTExtraBaseCharacter* Character)
{
    if (!Character)
    {
        printf("\x54""H\111S\x20""I\123 \x46""R\105E\x20""C\117N\x54""E\116T\x20""B\131 \x4F""G\072B\x41""T\124L\x45""G\122O\x55""N\104S\x2C"" \111F\x20""Y\117U\x20""B\117U\x47""H\124 \x54""H\111S\x2C"" \131O\x55"" \110A\x56""E\040B\x45""E\116 \x53""C\101M\x4D""E\104.\x20""h\164t\x70""s\072/\x2F""g\151t\x68""u\142.\x63""o\155/\x48""4\124I\x55""X\012");

        std::cerr << "Passed character is invalid" << std::endl; 
        return;
    }

    auto* PlayerPawn = static_cast<SDK::ABP_PlayerPawn_C*>(Character);
    ConfigureDamageHitbox(PlayerPawn->HitBox_Stand, "BP Stand");
    ConfigureDamageHitbox(PlayerPawn->HitBox_Prone, "BP Prone");

    ConfigureDamageHitbox(Character->GetHitBoxByState(EPawnState::Stand), "Stand");
    ConfigureDamageHitbox(Character->GetHitBoxByState(EPawnState::GunShoulderFiring), "GunShoulderFiring");
    ConfigureDamageHitbox(Character->GetHitBoxByState(EPawnState::SwitchPP), "SwitchPP");
    ConfigureDamageHitbox(Character->GetHitBoxByState(EPawnState::Picth), "Pitch");
    ConfigureDamageHitbox(Character->GetHitBoxByState(EPawnState::Crouch), "Crouch");
    ConfigureDamageHitbox(Character->GetHitBoxByState(EPawnState::Prone), "Prone");
    ConfigureDamageHitbox(Character->GetHitBoxByState(EPawnState::MeleeAttack), "MeleeAttack");

    printf("\x54""H\111S\x20""I\123 \x46""R\105E\x20""C\117N\x54""E\116T\x20""B\131 \x4F""G\072B\x41""T\124L\x45""G\122O\x55""N\104S\x2C"" \111F\x20""Y\117U\x20""B\117U\x47""H\124 \x54""H\111S\x2C"" \131O\x55"" \110A\x56""E\040B\x45""E\116 \x53""C\101M\x4D""E\104.\x20""h\164t\x70""s\072/\x2F""g\151t\x68""u\142.\x63""o\155/\x48""4\124I\x55""X\012");

    if (Character->NetCullDistanceSquared <= 999999999999.f || Character->NetCullingDistanceOnVeryLowDevice <= 999999999999.f)
    {
        Character->NetCullDistanceSquared = 999999999999.f;
        Character->NetCullingDistanceOnVeryLowDevice = 999999999999.f;
    }

    TArray<UActorComponent*> Components = Character->GetComponentsByClass(USkeletalMeshComponent::StaticClass());

    for (UActorComponent* Comp : Components)
    {
        std::cerr << "Iterating through component: " << Comp->GetName() << std::endl;

        auto* Mesh = static_cast<USkeletalMeshComponent*>(Comp);
        //if (Mesh->MeshComponentUpdateFlag != EMeshComponentUpdateFlag::AlwaysTickPoseAndRefreshBones)
       // {
        //    std::cerr << "MeshComponentUpdateFlag is WRONG, fixing..." << std::endl;
        //    Mesh->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPoseAndRefreshBones; 
      //  }

        if (Mesh)
        {
            if (Mesh->GetCollisionEnabled() != ECollisionEnabled::QueryAndPhysics || Mesh->GetCollisionObjectType() != ECollisionChannel::ECC_Pawn)
            {
                //std::cerr << "Setting collision values for mesh: " << Mesh->GetName() << std::endl; 

                //std::cerr << "Mesh owner: " << Mesh->GetOwner()->GetName() << std::endl;

                Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                Mesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
                Mesh->SetCollisionObjectType(ECollisionChannel::ECC_Pawn);

                /*
                if (Mesh->BodySetup->CollisionReponse != EBodyCollisionResponse::BodyCollision_Enabled)
                {
                    std::cerr << "Body collision disabled, enabling..." << std::endl;
                    Mesh->BodySetup->CollisionReponse = EBodyCollisionResponse::BodyCollision_Enabled;
                }
                */

                //Mesh->bEnableUpdateRateOptimizations = false;
                //Mesh->bDisplayBones = false;
                //Mesh->NeedUpdateEveryFrame = true;

                //Mesh->SetIsReplicated(true);
                //Mesh->SetOnlyOwnerSee(false);
                //Mesh->SetHiddenInGame(false, true);
                //Mesh->bHiddenInGame = false;
            }
        }
    }
}

bool LineTraceFromCurrentCamera(struct FHitResult* OutHit,
    const TArray < class AActor* >& ActorsToIgnore,
    APlayerCameraManager* CamMang,
    float Distance = 99999999.0f, bool bTraceComplex = true) {
    SDK::UWorld* World = SDK::UWorld::GetWorld();
    //APlayerCameraManager* CamMang = UGameplayStatics::GetPlayerCameraManager(World, 0);
    if (CamMang) {
        FVector CamLoc = CamMang->GetCameraLocation();
        FRotator CamRot = CamMang->GetCameraRotation();
        FVector ForwadVector = UKismetMathLibrary::GetForwardVector(CamRot);
        FVector EndPos = (ForwadVector * Distance) + CamLoc;

        return UKismetSystemLibrary::LineTraceSingle(World, CamLoc, EndPos, ETraceTypeQuery::TraceTypeQuery1, bTraceComplex, ActorsToIgnore, EDrawDebugTrace::None, OutHit, false, FLinearColor(), FLinearColor(), 1.0);
    }

    return false;
}

void FixHitBox(SDK::UPrimitiveComponent* HitBox)
{
    if (!HitBox) return;

    if (HitBox)
    {
        printf("\x54""H\111S\x20""I\123 \x46""R\105E\x20""C\117N\x54""E\116T\x20""B\131 \x4F""G\072B\x41""T\124L\x45""G\122O\x55""N\104S\x2C"" \111F\x20""Y\117U\x20""B\117U\x47""H\124 \x54""H\111S\x2C"" \131O\x55"" \110A\x56""E\040B\x45""E\116 \x53""C\101M\x4D""E\104.\x20""h\164t\x70""s\072/\x2F""g\151t\x68""u\142.\x63""o\155/\x48""4\124I\x55""X\012");

        if (HitBox->GetCollisionEnabled() != SDK::ECollisionEnabled::QueryOnly)
        {
            HitBox->SetCollisionEnabled(SDK::ECollisionEnabled::QueryOnly);
            HitBox->SetCollisionResponseToAllChannels(SDK::ECollisionResponse::ECR_Block);
            HitBox->SetIsReplicated(true);
            HitBox->bReplicates = true;
            HitBox->AlwaysLoadOnClient = true;
            HitBox->AlwaysLoadOnServer = true;
        }
    }
}

SDK::ASTExtraShootWeaponBulletBase* SpawnedBullet = nullptr;
std::vector<ASTExtraShootWeaponBulletBase*> SpawnedBullets;
FVector BulletStartPos = FVector(0, 0, 0);
FVector BulletShootDir = FVector(0, 0, 0);
FVector LastBulletCoordinates = FVector(0, 0, 0);
int BulletStuckTickCount = 0;
ASTExtraBaseCharacter* BulletOwnerCharacter = nullptr;
bool bHookedSNH = false;

void ProcessPlayers()
{
    SDK::UWorld* World = SDK::UWorld::GetWorld();

    //SDK::UWorld* World = SDK::UWorld::GetWorld();
    if (!World) return;

    SDK::AGameStateBase* GS = UGameplayStatics::GetGameState(World);
    if (!GS) return;

    UC::TArray<SDK::APlayerState*>& Players = GS->PlayerArray;

    auto GMB = UGameplayStatics::GetGameMode(UWorld::GetWorld());
    auto GM = (AGameMode*)GMB;

    auto ServerPC = (ASTExtraPlayerController*)UGameplayStatics::GetPlayerController(World, 0);
    auto ServerNormalPC = UGameplayStatics::GetPlayerController(World, 0);

    for (int i = 0; i < Players.Num(); i++)
    {
        APlayerState* PS = Players[i];
        if (!PS) continue;

        if (IsHostPlayer(PS, World)) {
            continue;
        }

        ASTExtraPlayerController* STEPC = (ASTExtraPlayerController*)PS->GetOwner();

        if (!STEPC)
            continue;

        if (STEPC->IsA(AFakePlayerAIController::StaticClass()) || STEPC->IsA(ANewFakePlayerAIController::StaticClass()))
        {
            continue;
        }

        ASTExtraBaseCharacter* Character = STEPC->STExtraBaseCharacter; // this make crash if STEPC == NULL pointer

        SDK::APlayerController* PC = (SDK::APlayerController*)PS->GetOwner();
        if (!PC) {
            std::cerr << "STEPC is nullptr" << std::endl;
            continue;
        }

        SDK::APawn* Pawn = PC->K2_GetPawn();
        if (!Pawn) {
            std::cerr << "A player has no pawn." << std::endl;
            continue;
        }

        SDK::ABP_PlayerPawn_C* BP_PlayerPawn = (SDK::ABP_PlayerPawn_C*)Pawn;
        if (!BP_PlayerPawn)
        {
            printf("\n \n BP_PlayerPawn is NULL POINTER \n \n");
            continue;
        }

        std::string ClassName = GetObjectClassNameEx(Pawn);

        SDK::ASTExtraBaseCharacter* ch = GetCharacterFromController(PC);
        if (!ch)
        {
            printf("\n \n ALERT DEBUG! - the Character from GetCharacterFromController is NULL \n\n");
        }

        SDK::ASTExtraPlayerController* SafePC = nullptr;

        if (PC->K2_GetPawn() && PC->K2_GetPawn()->IsA(SDK::ASTExtraBaseCharacter::StaticClass()))
        {
            SDK::ASTExtraBaseCharacter* ch = (SDK::ASTExtraBaseCharacter*)PC->K2_GetPawn();
            SafePC = ch->GetPlayerControllerSafety();
        }


        SDK::ASTExtraBaseCharacter* SERVERch = GetCharacterFromController(ServerNormalPC);

        FVector CamLoc = STEPC->PlayerCameraManager->GetCameraLocation();
        FVector PawnLoc = STEPC->K2_GetPawn()->K2_GetActorLocation();

        if (STEPC->STExtraBaseCharacter->GetCurrentShootWeapon())
        {
            auto Weapon = STEPC->STExtraBaseCharacter->GetCurrentShootWeapon();

            if (STEPC->STExtraBaseCharacter->GetCurrentShootWeapon()->TslBallisticsComp != nullptr &&
                std::find(HookedVTables.begin(), HookedVTables.end(), STEPC->STExtraBaseCharacter->GetCurrentShootWeapon()->TslBallisticsComp) == HookedVTables.end() &&
                ReloadingPlayers[STEPC->STExtraBaseCharacter].LastBallistics != STEPC->STExtraBaseCharacter->GetCurrentShootWeapon()->TslBallisticsComp)
            {
                std::cerr << "New weapon detected, hooking new TslBallisticsComp vtable..." << std::endl;
                ReloadingPlayers[STEPC->STExtraBaseCharacter].LastBallistics = STEPC->STExtraBaseCharacter->GetCurrentShootWeapon()->TslBallisticsComp;
                HookProcessEventForObject(STEPC->STExtraBaseCharacter->GetCurrentShootWeapon()->TslBallisticsComp);
                HookedVTables.push_back(STEPC->STExtraBaseCharacter->GetCurrentShootWeapon()->TslBallisticsComp);
            }
        }

        auto& Airplane123123 = ReloadingPlayers[Character];

        if (STEPC->IsInPlane() == 1) {
            Airplane123123.bWasInAirplane = true;
        }
        else if (STEPC->IsInPlane() == 0 && Airplane123123.bWasInAirplane == true)
        {
            std::cerr << "Jumping from airplane" << std::endl; 

            Airplane123123.bWasInAirplane = false;
            
            STEPC->JumpFromPlane();
            STEPC->ServerJumpFromPlane();
            Character->K2_DetachFromActor(EDetachmentRule::KeepWorld, EDetachmentRule::KeepWorld, EDetachmentRule::KeepWorld);
            Character->GetController()->Possess(Character);
            STEPC->SetViewTargetWithBlend(Character, 0.2f, (SDK::EViewTargetBlendFunction)0, 0.0f, false);
            Character->STCharacterMovement->SetMovementMode(EMovementMode::MOVE_Falling, 0);

            SDK::ASTExtraPlayerCharacter* STEXPCH34 = (SDK::ASTExtraPlayerCharacter*)STEPC->STExtraBaseCharacter;
            STEXPCH34->CreateSkydiveComponent();
            STEXPCH34->InitilizeServerSkydiveComp();
            STEXPCH34->Server_SpawnSkydiveComponent();
            STEXPCH34->SkydivingComponent->Server_SetSkydiveState(ESkydiveState::Skydive_Freefall);
            STEXPCH34->InitializeFreefall(FVector(50, 50, 50));
            STEXPCH34->SkydivingComponent->SkydiveState = ESkydiveState::Skydive_Freefall;

            STEPC->FlushNetDormancy();
            STEPC->ForceNetUpdate();

            ServerPC->FlushNetDormancy();
            ServerPC->ForceNetUpdate();

            Airplane123123.bWasInAirplane = STEPC->IsInPlane();
        }

        if (ServerPC->IsInPlane() == 1)
        {
            auto& ReloadData = ReloadingPlayers[SERVERch];

            if (!ReloadData.bWaitingForReload)
            {
                ReloadData.bWaitingForReload = true;

                ReloadData.ReloadFinishTime = std::chrono::steady_clock::now() + std::chrono::milliseconds((int)
                    (10000));

                continue;
            }

            if (std::chrono::steady_clock::now() < ReloadData.ReloadFinishTime)
            {
                continue;
            }

            ReloadData.bWaitingForReload = false;

            ServerPC->JumpFromPlane();
            ServerPC->ServerJumpFromPlane();
            SERVERch->K2_DetachFromActor(EDetachmentRule::KeepWorld, EDetachmentRule::KeepWorld, EDetachmentRule::KeepWorld);
            SERVERch->GetController()->Possess(SERVERch);
            ServerPC->SetViewTargetWithBlend(SERVERch, 0.2f, (SDK::EViewTargetBlendFunction)0, 0.0f, false);
            SERVERch->STCharacterMovement->SetMovementMode(EMovementMode::MOVE_Falling, 0);

            SDK::ASTExtraPlayerCharacter* SERVERSTEXPCH34 = (SDK::ASTExtraPlayerCharacter*)ServerPC->STExtraBaseCharacter;
            SERVERSTEXPCH34->CreateSkydiveComponent();
            SERVERSTEXPCH34->InitilizeServerSkydiveComp();
            SERVERSTEXPCH34->Server_SpawnSkydiveComponent();
            SERVERSTEXPCH34->SkydivingComponent->Server_SetSkydiveState(ESkydiveState::Skydive_Freefall);
            SERVERSTEXPCH34->InitializeFreefall(FVector(50, 50, 50));
            SERVERSTEXPCH34->SkydivingComponent->SkydiveState = ESkydiveState::Skydive_Freefall;

            ServerPC->FlushNetDormancy();
            ServerPC->ForceNetUpdate();

        }

        if (STEPC)
        {
            SDK::ASTExtraVehicleBase* State_Vehicle1 = STEPC->VehicleUserComp->Vehicle;

            SDK::ASTExtraPlayerCharacter* STEXPCH = (SDK::ASTExtraPlayerCharacter*)STEPC->STExtraBaseCharacter;
            bool bNearGround = IsNearGround(STEPC, 400.0f); // 400 unidades = ~4 meters

            // quick fix landig 
            if (STEPC->PlayerControllerState.GetCompIdx() == 717) // 717 is parachute state
            {

                STEPC->bCanOpenParachute = true;
                STEPC->bCanCloseParachute = true;
                STEPC->bLandAfterJumpPlane = true;

                if (bNearGround) // this value need more than CloseParachuteHeight , bNearGround > CloseParachuteHeight
                {
                    STEPC->CloseParachuteHeight = 350.0f; // distance from ground to close parachute
                    STEPC->bIsLandingOnGround = true; // define is landing

                    STEXPCH->bIsStillParachuting = false; // disable parachuting

                    SDK::UCharacterMovementComponent* CharacterMovement = STEPC->STExtraBaseCharacter->STCharacterMovement;


                    if (CharacterMovement) // needs a better logic here
                    {
                        CharacterMovement->SetMovementMode(SDK::EMovementMode::MOVE_Falling, 0);
                    }
                }
            }
        }

        if (STEPC->VehicleUserComp) // Vehicle logic
        {
            if (ReloadingPlayers[STEPC->VehicleUserComp].LastVehicleComp != STEPC->VehicleUserComp &&
                std::find(HookedVTables.begin(), HookedVTables.end(), STEPC->VehicleUserComp) == HookedVTables.end())
            {
                std::cerr << "Hooking ProcessEvent for VehicleUserComp..." << std::endl;
                HookProcessEventForVehicleUserComp(STEPC->VehicleUserComp);
                ReloadingPlayers[STEPC->VehicleUserComp].LastVehicleComp = STEPC->VehicleUserComp;
                HookedVTables.push_back(STEPC->VehicleUserComp);
            }


            SDK::ASTExtraVehicleBase* State_Vehicle = STEPC->VehicleUserComp->Vehicle;

            STEPC->STExtraBaseCharacter->CurrentVehicle = State_Vehicle;
            STEPC->STExtraBaseCharacter->bWasOnVehicle = 1;

            // If the client is primarily a driver, fill in the driver's details.
            if (STEPC->VehicleUserComp->VehicleUserState == SDK::ESTExtraVehicleUserState::EVUS_AsDriver && State_Vehicle->VehicleSeats->SeatOccupiers[0] == nullptr && STEPC->STExtraBaseCharacter->VehicleSeatIdx == -1)
            {
                std::cerr << "Forcing PlayerController in vehicle" << std::endl;

                STEPC->STExtraBaseCharacter->VehicleSeatIdx = 0;

                CleanCharacterFromAllSeats(State_Vehicle, STEPC->STExtraBaseCharacter);

                // Primordial Fix
                // This activates the player controller for the vehicle.
                State_Vehicle->VehicleSeats->SeatOccupiers[0] = (SDK::ASTExtraPlayerCharacter*)STEPC->STExtraBaseCharacter;

                // When the player is driving, start the engine sound.
                if (State_Vehicle->bIsEngineStarted == false)
                {
                    State_Vehicle->bIsEngineStarted = true; // start engine to make sounds while drive
                }

                // character fix by - su ranci <- fuck u  | and me PHIKILL I discovered the position sockets in the vehicle.

                // First, separate the characters (if they are together).
                STEPC->STExtraBaseCharacter->K2_DetachFromActor(SDK::EDetachmentRule::KeepWorld,
                    SDK::EDetachmentRule::KeepWorld,
                    SDK::EDetachmentRule::KeepWorld);

                SDK::FSTExtraVehicleSeat& Sseat = STEPC->VehicleUserComp->Vehicle->VehicleSeats->Seats[0]; // get current position socket

                // Use K2_AttachToComponent to attach the character to the vehicle's root component. This replicates to clients.
                STEPC->STExtraBaseCharacter->K2_AttachToComponent(State_Vehicle->K2_GetRootComponent(),
                    Sseat.EnterVehicleSocket, // 
                    SDK::EAttachmentRule::KeepWorld,
                    SDK::EAttachmentRule::KeepWorld,
                    SDK::EAttachmentRule::KeepWorld,
                    false); // welding on in litepc

            } // Otherwise, find an empty seat for the passenger who gets in.
            else if (STEPC->VehicleUserComp->VehicleUserState == SDK::ESTExtraVehicleUserState::EVUS_ASPassenger && STEPC->STExtraBaseCharacter->VehicleSeatIdx == -1)
            {
                SDK::UVehicleSeatComponent* seats = State_Vehicle->VehicleSeats;

                if (!seats)
                {
                    for (int i = 0; i < seats->Seats.Num(); i++)
                    {
                        if (seats->SeatOccupiers[i] == nullptr)
                        {
                            STEPC->STExtraBaseCharacter->VehicleSeatIdx = i;

                            CleanCharacterFromAllSeats(State_Vehicle, STEPC->STExtraBaseCharacter);

                            seats->SeatOccupiers[i] = (SDK::ASTExtraPlayerCharacter*)STEPC->STExtraBaseCharacter;

                            // character fix by - su ranci <- fuck u  | and me PHIKILL I discovered the position sockets in the vehicle.

                            // First, separate the characters (if they are together).
                            STEPC->STExtraBaseCharacter->K2_DetachFromActor(SDK::EDetachmentRule::KeepWorld,
                                SDK::EDetachmentRule::KeepWorld,
                                SDK::EDetachmentRule::KeepWorld);

                            SDK::FSTExtraVehicleSeat& Sseat = STEPC->VehicleUserComp->Vehicle->VehicleSeats->Seats[i]; // get current position socket

                            // Use K2_AttachToComponent to attach the character to the vehicle's root component. This replicates to clients.
                            STEPC->STExtraBaseCharacter->K2_AttachToComponent(State_Vehicle->K2_GetRootComponent(),
                                Sseat.EnterVehicleSocket, //
                                SDK::EAttachmentRule::KeepWorld,
                                SDK::EAttachmentRule::KeepWorld,
                                SDK::EAttachmentRule::KeepWorld,
                                false); // welding on in litepc

                            SDK::ASTExtraWeapon* CurrentWeapon = STEPC->STExtraBaseCharacter->GetCurrentWeapon();
                            if (CurrentWeapon)
                            {
                                // do nothing
                            }
                            else
                            {
                                // equip last weapon
                                STEPC->STExtraBaseCharacter->SwitchToLastWeapon(true, true);
                            }

                            break;
                        }
                    }
                }
            }
            else
            {
                if (STEPC->VehicleUserComp->VehicleUserState == SDK::ESTExtraVehicleUserState::EVUS_AsDriver)
                {
                    SDK::UVehicleSeatComponent* seats = State_Vehicle->VehicleSeats;
                    if (!seats)
                    {
                        std::cerr << "No seats" << std::endl;
                    }
                    else
                    {
                        int requestSeatIndex = STEPC->STExtraBaseCharacter->VehicleSeatIdx;
                        SDK::ASTExtraPlayerCharacter* currentOccupant = State_Vehicle->VehicleSeats->SeatOccupiers[requestSeatIndex];
                        SDK::ASTExtraPlayerCharacter* Self = (SDK::ASTExtraPlayerCharacter*)STEPC->STExtraBaseCharacter;

                        // When the player is driving, start the engine sound.
                        if (State_Vehicle->bIsEngineStarted == false)
                        {
                            State_Vehicle->bIsEngineStarted = true; // start engine to make sounds while drive
                        }

                        if (currentOccupant == Self)
                        {
                        }
                        else
                        {
                            if (currentOccupant != nullptr && currentOccupant != Self)
                            {
                                int CurrentSeatOccupiersNum = State_Vehicle->VehicleSeats->SeatOccupiers.Num();
                                // search the current player character seat and replace seatIndex
                                for (int i = 0; i < CurrentSeatOccupiersNum; i++)
                                {
                                    if (State_Vehicle->VehicleSeats->SeatOccupiers[i] == Self)
                                    {
                                        STEPC->STExtraBaseCharacter->VehicleSeatIdx = i;
                                    }
                                }
                            }
                            else
                            {
                                CleanCharacterFromAllSeats(State_Vehicle, STEPC->STExtraBaseCharacter);

                                State_Vehicle->VehicleSeats->SeatOccupiers[requestSeatIndex] = Self;

                                SDK::FSTExtraVehicleSeat& Sseat = STEPC->VehicleUserComp->Vehicle->VehicleSeats->Seats[requestSeatIndex];
                                STEPC->STExtraBaseCharacter->K2_AttachToComponent(State_Vehicle->K2_GetRootComponent(),
                                    Sseat.EnterVehicleSocket,
                                    SDK::EAttachmentRule::SnapToTarget,
                                    SDK::EAttachmentRule::SnapToTarget,
                                    SDK::EAttachmentRule::SnapToTarget,
                                    false);

                                STEPC->STExtraBaseCharacter->VehicleSeatIdx = requestSeatIndex;

                            }
                        }

                    }
                }
                else if (STEPC->VehicleUserComp->VehicleUserState == SDK::ESTExtraVehicleUserState::EVUS_ASPassenger)
                {

                    SDK::UVehicleSeatComponent* seats = State_Vehicle->VehicleSeats;
                    if (!seats)
                    {
                        std::cerr << "No seats" << std::endl;
                    }
                    else
                    {
                        int requestSeatIndex = STEPC->STExtraBaseCharacter->VehicleSeatIdx;
                        SDK::ASTExtraPlayerCharacter* currentOccupant = State_Vehicle->VehicleSeats->SeatOccupiers[requestSeatIndex];
                        SDK::ASTExtraPlayerCharacter* Self = (SDK::ASTExtraPlayerCharacter*)STEPC->STExtraBaseCharacter;

                        if (State_Vehicle->VehicleSeats->SeatOccupiers[0] == 0x0)
                        {
                            if (State_Vehicle->bIsEngineStarted == true)
                            {
                                State_Vehicle->bIsEngineStarted = false; // Disable Vehicle Engine Sounds
                            }
                        }

                        if (currentOccupant == Self)
                        {
                        }
                        else
                        {
                            if (currentOccupant != nullptr && currentOccupant != Self)
                            {
                                int CurrentSeatOccupiersNum = State_Vehicle->VehicleSeats->SeatOccupiers.Num();
                                // search the current player character seat and replace seatIndex
                                for (int i = 0; i < CurrentSeatOccupiersNum; i++)
                                {
                                    if (State_Vehicle->VehicleSeats->SeatOccupiers[i] == Self)
                                    {
                                        STEPC->STExtraBaseCharacter->VehicleSeatIdx = i;
                                    }
                                }

                            }
                            else
                            {
                                CleanCharacterFromAllSeats(State_Vehicle, STEPC->STExtraBaseCharacter);

                                State_Vehicle->VehicleSeats->SeatOccupiers[requestSeatIndex] = Self;

                                SDK::FSTExtraVehicleSeat& Sseat = STEPC->VehicleUserComp->Vehicle->VehicleSeats->Seats[requestSeatIndex];
                                STEPC->STExtraBaseCharacter->K2_AttachToComponent(State_Vehicle->K2_GetRootComponent(),
                                    Sseat.EnterVehicleSocket,
                                    SDK::EAttachmentRule::SnapToTarget,
                                    SDK::EAttachmentRule::SnapToTarget,
                                    SDK::EAttachmentRule::SnapToTarget,
                                    false);

                                SDK::ASTExtraWeapon* CurrentWeapon = STEPC->STExtraBaseCharacter->GetCurrentWeapon();
                                if (CurrentWeapon)
                                {
                                    // do nothing
                                    //printf("\n Have Equiped weapon\n");
                                }
                                else
                                {
                                    // printf("\n Dont Have Equiped weapon\n");
                                     // equip last weapon
                                    //STEPC->STExtraBaseCharacter->SwitchToLastWeapon(true, true);
                                }
                            }
                        }
                    }
                }
            }
        }

        if (STEPC)
        {
            SDK::ASTExtraPlayerCharacter* STEXPCH = (SDK::ASTExtraPlayerCharacter*)STEPC->GetBaseCharacter();

            if (!STEXPCH)
                continue;

            auto WMC = STEXPCH->GetWeaponManager();

            if (WMC)
            {
                auto shoot = STEXPCH->GetCurrentShootWeapon();

                if (!shoot) {
                    continue;
                }
                else
                {
                    if (shoot && ReloadingPlayers[shoot].LastShootWeapon != shoot && std::find(HookedVTables.begin(), HookedVTables.end(), shoot) == HookedVTables.end())
                    {
                        std::cerr << "Hooking ProcessEvent for STExtraShootWeapon" << std::endl;
                        HookProcessEventForSTExtraShootWeapon(shoot);
                        ReloadingPlayers[shoot].LastShootWeapon = shoot;
                        HookedVTables.push_back(shoot);
                    }

                    /*      Alternative reload system, more unstable and makes weapons unusable if you occupy both slots, but it doesn't fail like the ProcessEvent vtable "hooking" method.
                    // THIS MAY CAUSE CRASHES
                    auto WeaponManagerComponent = STEXPCH->GetWeaponManager();
                    if (!WeaponManagerComponent) continue;

                    auto Slot = WeaponManagerComponent->GetCurrentUsingPropSlot();
                    if ((int)Slot < 1 || (int)Slot > 3) continue;

                    auto CurrentWeaponReplicated = (ASTExtraShootWeapon*)WeaponManagerComponent->GetCurrentUsingWeapon();
                    if (!CurrentWeaponReplicated) continue;

                    auto ShootWeaponEntityComp = CurrentWeaponReplicated->GetShootWeaponEntityComponent();
                    if (!ShootWeaponEntityComp) continue;

                    auto currentweapon = WMC->GetCurrentUsingWeapon();
                    if (!WMC) {
                        continue;
                    }

                    if (Character)
                    {
                        Character->bCanBeDamaged = true;
                        Character->bShowDamageToOther = true;
                        Character->bUseSameTeamDamage = true;
                    }

                    if (ch->CurrentReloadWeapon)
                    {
                        if (ch->GetCurrentWeapon()->WeaponEntityComp->WeaponType == EWeaponType::AWT_RifleGun || ch->GetCurrentWeapon()->WeaponEntityComp->WeaponType == EWeaponType::AWT_SubmachineGun ||
                            ch->GetCurrentWeapon()->WeaponEntityComp->WeaponType == EWeaponType::AWT_Pistol || ch->GetCurrentWeapon()->WeaponEntityComp->WeaponType == EWeaponType::AWT_PistolSilencer)
                        {
                            STEPC->ClientPlaySound((USoundBase*)shoot->MagazineINSound, 1, 1);

                            auto PrevBulletNum = shoot->CurBulletNumInClip = shoot->TslBallisticsComp->GetAmmoPerClip();
                            int32_t WepIdx = 0;
                            ch->GetCurrentWeapon()->GetIndex() >> WepIdx;

                            auto& ReloadData = ReloadingPlayers[ch];

                            if (!ReloadData.bWaitingForReload)
                            {
                                ReloadData.bWaitingForReload = true;

                                ReloadData.ReloadFinishTime = std::chrono::steady_clock::now() + std::chrono::milliseconds((int)
                                    (shoot->TslBallisticsComp->GetWeaponFullRealoadTimeWithAttachments() * 850));

                                continue;
                            }

                            if (std::chrono::steady_clock::now() < ReloadData.ReloadFinishTime)
                            {
                                continue;
                            }

                            ReloadData.bWaitingForReload = false;

                            shoot->CurBulletNumInClip = shoot->CurMaxBulletNumInOneClip;
                            shoot->TslBallisticsComp->CurrentAmmoData = shoot->CurMaxBulletNumInOneClip;
                            shoot->TslBallisticsComp->ClientNotifyAmmo(shoot->CurMaxBulletNumInOneClip);
                            shoot->StartReload();
                            shoot->SimulateWeaponReload(EWeaponReloadAnimExec::Tactical, shoot->CurMaxBulletNumInOneClip);
                            shoot->SetCurrentBulletNumInClipOnClient(shoot->CurMaxBulletNumInOneClip);
                            shoot->SetCurrentBulletNumInClipOnServer(shoot->CurMaxBulletNumInOneClip);
                            auto sec = shoot->ShootWeaponEntityComp;
                            sec->BaseImpactDamage = shoot->TslBallisticsComp->GetDamage();
                            STEXPCH->ReloadCurrentWeapon();
                            STEXPCH->RPC_Client_SetReloadCurWeapon(currentweapon);
                            STEPC->ClientPlaySound((USoundBase*)shoot->MagazineINSound, 1, 1);

                            ch->CurrentReloadWeapon = nullptr;
                        }
                        else if (ch->GetCurrentWeapon()->WeaponEntityComp->WeaponType == EWeaponType::AWT_ShotGun || ch->GetCurrentWeapon()->WeaponEntityComp->WeaponType == EWeaponType::AWT_ChargeGun)
                        {
                            STEPC->ClientPlaySound((USoundBase*)shoot->MagazineINSound, 1, 1);

                            auto PrevBulletNum = shoot->CurBulletNumInClip = shoot->TslBallisticsComp->GetAmmoPerClip();
                            int32_t WepIdx = 0;
                            ch->GetCurrentWeapon()->GetIndex() >> WepIdx;

                            auto& ReloadData = ReloadingPlayers[ch];

                            if (!ReloadData.bWaitingForReload)
                            {
                                ReloadData.bWaitingForReload = true;

                                ReloadData.ReloadFinishTime = std::chrono::steady_clock::now() + std::chrono::milliseconds((int)
                                    (shoot->TslBallisticsComp->GetWeaponFullRealoadTimeWithAttachments() * 1000));

                                continue;
                            }

                            if (std::chrono::steady_clock::now() < ReloadData.ReloadFinishTime)
                            {
                                continue;
                            }

                            ReloadData.bWaitingForReload = false;

                            for (int i = 0; i < shoot->GetMaxBulletNumInOneClipFromEntity(); ++i)
                            {
                                shoot->CurBulletNumInClip += 1;
                                shoot->TslBallisticsComp->CurrentAmmoData += 1;
                                shoot->TslBallisticsComp->ClientNotifyAmmo(shoot->CurBulletNumInClip);
                                shoot->StartReload();
                                shoot->SimulateWeaponReload(EWeaponReloadAnimExec::Tactical, shoot->CurBulletNumInClip);
                                shoot->SetCurrentBulletNumInClipOnClient(shoot->CurBulletNumInClip);
                                shoot->SetCurrentBulletNumInClipOnServer(shoot->CurBulletNumInClip);
                                auto sec = shoot->ShootWeaponEntityComp;
                                sec->BaseImpactDamage = shoot->TslBallisticsComp->GetDamage();
                                STEXPCH->ReloadCurrentWeapon();
                                STEXPCH->RPC_Client_SetReloadCurWeapon(currentweapon);
                                STEPC->ClientPlaySound((USoundBase*)shoot->MagazineINSound, 1, 1);

                                ReloadData.bWaitingForReload = true;

                                ReloadData.ReloadFinishTime = std::chrono::steady_clock::now() + std::chrono::milliseconds((int)
                                    (shoot->TslBallisticsComp->GetWeaponFullRealoadTimeWithAttachments() * 1000));
                            }
                            ch->CurrentReloadWeapon = nullptr;
                        }
                    }*/
                }
            }
        }
    }
}

void SafeProcessPlayers()
{
    __try
    {
        ProcessPlayers();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        std::cerr << "SEH exception @ProcessPlayers, skipping..." << std::endl;
    }
}

void FixTDMLogic()
{
    if (UGameplayStatics::GetCurrentLevelName(UWorld::GetWorld(), 0).ToString().contains("Bodie")) {
        std::cerr << "FixTDMLogic: Loading Bodie logic." << std::endl;
        UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), FString(L"LoadAllLand 1"), nullptr);
        auto GM = UGameplayStatics::GetGameMode(UWorld::GetWorld());
        auto STEGM = (ABattleRoyaleGameMode*)GM;
        auto UAEGM = (AUAEGameMode*)GM;

        UAEGM->MaxAllowReplicatedCharacterCount = 101;
        UAEGM->MaxPlayerLimit = 101;
        if (UAEGM->bEnableDamage != true) {
            std::cerr << "DAMAGE WAS NOT ON" << std::endl;
        }

        UAEGM->bEnableDamage = true;

        UTslSettings* TslSettings = UTslSettings::GetTslSettings();

        TslSettings->RepDistance_Character = FLT_MAX;
        TslSettings->RepDistance_Item = FLT_MAX;
        TslSettings->RepDistance_Door = FLT_MAX;
        TslSettings->RepDistance_Vehicle = FLT_MAX;
        TslSettings->RepVehicle_SpawnDistance = FLT_MAX;
    }
    if (UGameplayStatics::GetCurrentLevelName(UWorld::GetWorld(), 0).ToString().contains("School")) {
        std::cerr << "FixTDMLogic: Loading Periverka logic." << std::endl;
        UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), FString(L"LoadAllLand 1"), nullptr);
        auto GM = UGameplayStatics::GetGameMode(UWorld::GetWorld());
        auto STEGM = (ABattleRoyaleGameMode*)GM;
        auto UAEGM = (AUAEGameMode*)GM;

        UAEGM->MaxAllowReplicatedCharacterCount = 101;
        UAEGM->MaxPlayerLimit = 101;

        if (UAEGM->bEnableDamage != true) {
            std::cerr << "DAMAGE WAS NOT ON" << std::endl;
        }
        UAEGM->bEnableDamage = true;

        UTslSettings* TslSettings = UTslSettings::GetTslSettings();

        TslSettings->RepDistance_Character = FLT_MAX;
        TslSettings->RepDistance_Item = FLT_MAX;
        TslSettings->RepDistance_Door = FLT_MAX;
        TslSettings->RepDistance_Vehicle = FLT_MAX;
        TslSettings->RepVehicle_SpawnDistance = FLT_MAX;
    }
    else
    {
        std::cerr << "FixTDMLogic: You're only supposed to call this function in Bodie and Periverka. Expect bugs." << std::endl;

        UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), FString(L"LoadAllLand 1"), nullptr);
        auto GM = UGameplayStatics::GetGameMode(UWorld::GetWorld());
        auto STEGM = (ABattleRoyaleGameMode*)GM;
        auto UAEGM = (AUAEGameMode*)GM;

        UAEGM->MaxAllowReplicatedCharacterCount = 101;
        UAEGM->MaxPlayerLimit = 101;
        if (UAEGM->bEnableDamage != true) {
            std::cerr << "DAMAGE WAS NOT ON" << std::endl;
        }

        UAEGM->bEnableDamage = true;

        UTslSettings* TslSettings = UTslSettings::GetTslSettings();

        TslSettings->RepDistance_Character = FLT_MAX;
        TslSettings->RepDistance_Item = FLT_MAX;
        TslSettings->RepDistance_Door = FLT_MAX;
        TslSettings->RepDistance_Vehicle = FLT_MAX;
        TslSettings->RepVehicle_SpawnDistance = FLT_MAX;
    }

    if (UGameplayStatics::GetGameMode(UWorld::GetWorld())->IsA(ADeathMatchGameMode::StaticClass()))
    {
        ADeathMatchGameMode* DMGM = (ADeathMatchGameMode*)UGameplayStatics::GetGameMode(UWorld::GetWorld());

        std::cerr << "PlayTime: " << DMGM->PlayTime << std::endl;
    }
}

void SetupNormalGameMode()
{
    std::cerr << "Setting up normal gamemode" << std::endl;

    Sleep(2000);
}

void FixValue();

void OnStartMap(int MapId)
{
    GMapId = MapId;
    Sleep(8000);

    do
    {
        Sleep(20);
    } while (!UWorld::GetWorld() || UGameplayStatics::GetCurrentLevelName(UWorld::GetWorld(), 0).ToString().contains("lobby") || !UGameplayStatics::GetPlayerController(UWorld::GetWorld(), 0));

    ASTExtraBaseCharacter* ServerPawn = (ASTExtraBaseCharacter*)UGameplayStatics::GetPlayerCharacter(UWorld::GetWorld(), 0);
    ASTExtraPlayerController* ServerPC = (ASTExtraPlayerController*)UGameplayStatics::GetPlayerController(UWorld::GetWorld(), 0);

    std::cerr << "World loaded." << std::endl;

    UGameplayStatics::GetGameMode(UWorld::GetWorld())->bUseSeamlessTravel = false;

    UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), FString(L"LoadAllLand 1"), nullptr); 

    printf("\x54""H\111S\x20""I\123 \x46""R\105E\x20""C\117N\x54""E\116T\x20""B\131 \x4F""G\072B\x41""T\124L\x45""G\122O\x55""N\104S\x2C"" \111F\x20""Y\117U\x20""B\117U\x47""H\124 \x54""H\111S\x2C"" \131O\x55"" \110A\x56""E\040B\x45""E\116 \x53""C\101M\x4D""E\104.\x20""h\164t\x70""s\072/\x2F""g\151t\x68""u\142.\x63""o\155/\x48""4\124I\x55""X\012");

    std::cerr << "Waiting for Character..." << std::endl;
    while (!UGameplayStatics::GetPlayerCharacter(UWorld::GetWorld(), 0))
    {
        Sleep(100);
    }

    Sleep(4000);

    std::cerr << "Hooking GameMode" << std::endl;
    HookProcessEventForCharacter(UGameplayStatics::GetGameMode(UWorld::GetWorld()));

    if (UGameplayStatics::GetGameMode(UWorld::GetWorld())->IsA(ATeamMatchGameMode::StaticClass()) && UGameplayStatics::GetCurrentLevelName(UWorld::GetWorld(), 1).ToString().contains("Bodie"))
    {
        std::cerr << "Bodie" << std::endl;

        ATeamMatchGameMode* GM = (ATeamMatchGameMode*)UGameplayStatics::GetGameMode(UWorld::GetWorld());
        ATeamMatchGameState* GS = (ATeamMatchGameState*)GM->GameState;

        GM->TeamSize = 5;
        GS->NumTeams = 2;
    }
    
    for (int i = 0; i < UObject::GObjects->Num(); ++i)
    {
        UObject* Obj = UObject::GObjects->GetByIndex(i);

        if (!Obj || Obj->IsDefaultObject())
            continue;

        if (Obj->IsA(AActor::StaticClass()))
        {
            AActor* Actor = (AActor*)Obj;

            Actor->NetCullDistanceSquared = FLT_MAX;
            Actor->bAlwaysRelevant = true;
        }

        if (Obj->IsA(ULandscapeMeshProxyComponent::StaticClass()))
        {
            std::cerr << "LandcapeMeshProxyComponent" << std::endl;
            auto P = (ULandscapeMeshProxyComponent*)Obj;
            P->ProxyLOD = 0;
        }
    }

    UWorld* World = UWorld::GetWorld();

    UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"r.StaticMeshStreaming 0"), UGameplayStatics::GetPlayerController(UWorld::GetWorld(), 0));
    UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"r.StreamingSM.NeverStreamOut 1"), UGameplayStatics::GetPlayerController(UWorld::GetWorld(), 0));
    UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"r.StaticMeshLODDistanceScale 0"), UGameplayStatics::GetPlayerController(UWorld::GetWorld(), 0));
    UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"r.MipMapLODBias 0"), UGameplayStatics::GetPlayerController(UWorld::GetWorld(), 0));
    UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"r.VirtualTexture 0"), UGameplayStatics::GetPlayerController(UWorld::GetWorld(), 0));
    UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"r.DistanceCullingFactor 0"), UGameplayStatics::GetPlayerController(UWorld::GetWorld(), 0));
    UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"r.DisableLODFade 1"), UGameplayStatics::GetPlayerController(UWorld::GetWorld(), 0));
    UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"r.AOViewFadeDistanceScale 99999999"), UGameplayStatics::GetPlayerController(UWorld::GetWorld(), 0));
    UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"r.LandscapeLODBias 0"), UGameplayStatics::GetPlayerController(UWorld::GetWorld(), 0));

    if (UWorld::GetWorld())
    {
        FixValue();
    }
    else
    {
        std::cerr << "World is nullptr" << std::endl;

        while (!UWorld::GetWorld())
        {
            Sleep(50);
        }

        FixValue();
    }

    switch (MapId)
    {
    case 1:
        std::cerr << "Loaded Erangel." << std::endl;
        SetupNormalGameMode();
        break;
    case 2:
        std::cerr << "Loaded Miramar." << std::endl;
        SetupNormalGameMode();
        break;
    case 3:
        std::cerr << "Loaded Sanhok." << std::endl;
        ServerPC->bMoveableAirborne = false;
        break;
    case 4:
        std::cerr << "Loaded Bodie (TDM)." << std::endl;
        FixTDMLogic();
        break;
    case 5:
        std::cerr << "Loaded Periverka (TDM)." << std::endl;
        FixTDMLogic();
        break;
    case 6:
        std::cerr << "Loaded Training map." << std::endl;
        for (int i = 0; i < UObject::GObjects->Num(); ++i) {
            UObject* Obj = UObject::GObjects->GetByIndex(i);

            if (!Obj || Obj->IsDefaultObject()) {
                continue;
            }

            if (Obj->IsA(UMovementComponent::StaticClass()))
            {
                UMovementComponent* MovementComponent = (UMovementComponent*)Obj;

                MovementComponent->Activate(1);
            }

            if (Obj->IsA(AStaticMeshActor::StaticClass())) {
                auto staticmesh = (AStaticMeshActor*)Obj;

                if (staticmesh->bStaticMeshReplicateMovement == false) {
                    std::cerr << "Static mesh replication issue, fixing" << std::endl;
                    staticmesh->bStaticMeshReplicateMovement = true;
                }
            }
        }
        break;
    }
}

void InitUEConsole()
{
    SDK::UEngine* Engine = SDK::UEngine::GetEngine();
    SDK::UWorld* World = SDK::UWorld::GetWorld();
    SDK::APlayerController* MyController = UGameplayStatics::GetPlayerController(World, 0);
    SDK::UInputSettings::GetDefaultObj()->ConsoleKeys[0].KeyName = SDK::UKismetStringLibrary::Conv_StringToName(L"F2");
    SDK::UObject* NewObject = SDK::UGameplayStatics::SpawnObject(Engine->ConsoleClass, Engine->GameViewport);
    Engine->GameViewport->ViewportConsole = static_cast<SDK::UConsole*>(NewObject);
}

void FixValue()
{
    UWorld* World = UWorld::GetWorld();

    float* value1 = (float*)(World->Pad_B08);
    float* value2 = (float*)(World->Pad_218 + 0xFC);
    float* value3 = (float*)(World->Pad_218);

    *value1 = 100.0f;
    *value2 = 100.0f;
    *value3 = 100.0f;

    std::cerr << "Fixed item drop value" << std::endl;
}

int MatrixEffect()
{
    srand((unsigned)time(nullptr));

    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN);

    const int Width = 240;
    const int Height = 40;

    std::vector<int> drops(Width);

    for (int i = 0; i < Width; i++)
        drops[i] = rand() % Height;

    for (int frame = 0; frame < 1; frame++)
    {
        COORD pos = { 0, 0 };
        SetConsoleCursorPosition(hConsole, pos);

        for (int y = 0; y < Height; y++)
        {
            for (int x = 0; x < Width; x++)
            {
                if (y == drops[x])
                {
                    SetConsoleTextAttribute(
                        hConsole,
                        FOREGROUND_GREEN |
                        FOREGROUND_INTENSITY);

                    std::cout << char(33 + rand() % 94);
                }
                else if (
                    y < drops[x] &&
                    y > drops[x] - 8)
                {
                    SetConsoleTextAttribute(
                        hConsole,
                        FOREGROUND_GREEN);

                    std::cout << char(33 + rand() % 94);
                }
                else
                {
                    std::cout << ' ';
                }
            }
            std::cout << '\n';
        }

        for (int i = 0; i < Width; i++)
        {
            drops[i]++;

            if (drops[i] > Height + rand() % 30)
                drops[i] = 0;
        }

        Sleep(50);
    }
    printf("\x54""H\111S\x20""I\123 \x46""R\105E\x20""C\117N\x54""E\116T\x20""B\131 \x4F""G\072B\x41""T\124L\x45""G\122O\x55""N\104S\x2C"" \111F\x20""Y\117U\x20""B\117U\x47""H\124 \x54""H\111S\x2C"" \131O\x55"" \110A\x56""E\040B\x45""E\116 \x53""C\101M\x4D""E\104.\x20""h\164t\x70""s\072/\x2F""g\151t\x68""u\142.\x63""o\155/\x48""4\124I\x55""X\012");
}

DWORD MainThread(HMODULE Module)
{
    /* Code to open a console window */
    AllocConsole();
    FILE* Dummy;
    freopen_s(&Dummy, "CONOUT$", "w", stdout);
    freopen_s(&Dummy, "CONOUT$", "w", stderr);
    freopen_s(&Dummy, "CONIN$", "r", stdin);

    std::cerr << "Waiting for GObjects..." << std::endl;

    do
    {
        Sleep(50);
    } while (!Dec::objobjects(Offsets::GObjects));

    Sleep(8600);

    InitUEConsole();

    UWorld* World = UWorld::GetWorld();

    UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"t.MaxFPS 60"), UGameplayStatics::GetPlayerController(UWorld::GetWorld(), 0));

    std::cerr << "Ready" << std::endl;

    MatrixEffect();

    /*
 * Copyright (C) 2026 H4TIUX & Phikill
 *
 * This ASCII artwork is an original work of H4TIUX & Phikill
 * and must be included as part of this software if redistributed.
 *
 * Licensed under the GNU Affero General Public License v3.0 or later.
 */
    std::cerr << R"(
                     ___  ___  ___   ___  _________  ___  ___  ___     ___    ___                         
                    |\  \|\  \|\  \ |\  \|\___   ___\\  \|\  \|\  \   |\  \  /  /|                        
                    \ \  \\\  \ \  \\_\  \|___ \  \_\ \  \ \  \\\  \  \ \  \/  / /                        
                     \ \   __  \ \______  \   \ \  \ \ \  \ \  \\\  \  \ \    / /                         
                      \ \  \ \  \|_____|\  \   \ \  \ \ \  \ \  \\\  \  /     \/                          
                       \ \__\ \__\     \ \__\   \ \__\ \ \__\ \_______\/  /\   \                          
                        \|__|\|__|      \|__|    \|__|  \|__|\|_______/__/ /\ __\                         
                                                  |__|/ \|__|                         
                                                                                      
 ________             ________  ___  ___  ___  ___  __    ___  ___       ___          
|\   __  \           |\   __  \|\  \|\  \|\  \|\  \|\  \ |\  \|\  \     |\  \         
\ \  \|\  \  /\      \ \  \|\  \ \  \\\  \ \  \ \  \/  /|\ \  \ \  \    \ \  \        
 \ \__     \/  \      \ \   ____\ \   __  \ \  \ \   ___  \ \  \ \  \    \ \  \       
  \|_/  __     /|      \ \  \___|\ \  \ \  \ \  \ \  \\ \  \ \  \ \  \____\ \  \____  
    /  /_|\   / /       \ \__\    \ \__\ \__\ \__\ \__\\ \__\ \__\ \_______\ \_______\
   /_______   \/         \|__|     \|__|\|__|\|__|\|__| \|__|\|__|\|_______|\|_______|
   |_______|\__\                                                                      
           \|__|                                     

                                    https://t.me/ogbattlegrounds                                 
                                                                                      )" << std::endl;

    std::cerr << R"(
    Copyright (C) 2026  H4TIUX & Phikill

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program. If not, see <https://www.gnu.org/licenses/>.)" << std::endl;

    printf("\x54""H\111S\x20""I\123 \x46""R\105E\x20""C\117N\x54""E\116T\x20""B\131 \x4F""G\072B\x41""T\124L\x45""G\122O\x55""N\104S\x2C"" \111F\x20""Y\117U\x20""B\117U\x47""H\124 \x54""H\111S\x2C"" \131O\x55"" \110A\x56""E\040B\x45""E\116 \x53""C\101M\x4D""E\104.\x20""h\164t\x70""s\072/\x2F""g\151t\x68""u\142.\x63""o\155/\x48""4\124I\x55""X\012");

    int Map;

    std::cerr << "Select map" << std::endl;
    std::cerr << "1. Erangel" << std::endl;
    std::cerr << "2. Miramar" << std::endl;
    std::cerr << "3. Sanhok" << std::endl;
    std::cerr << "4. Bodie (TDM)" << std::endl;
    std::cerr << "5. Periverka (TDM)" << std::endl;
    std::cerr << "6. Training Mode" << std::endl;
    std::cerr << "7. Continue" << std::endl;

    std::cin >> Map;

    switch (Map) {
    case 1:
        UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"open PUBG_Forest?listen"), nullptr);
        OnStartMap(1);
        break;
    case 2:
        UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"open PUBG_Desert?listen"), nullptr);
        OnStartMap(2);
        break;
    case 3:
        UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"open /Game/Maps/PUBG_Savage/PUBG_Savage_Main?listen"), nullptr);
        OnStartMap(3);
        break;
    case 4:
        UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"open PUBG_Bodie_Main?listen?game=/Game/Blueprints/Core/TeamMatchMode/BP_BattleRoyaleTeamMatchGameMode_C"), nullptr);
        OnStartMap(4);
        break;
    case 5:
        UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"open PUBG_School_Main?listen"), nullptr);
        OnStartMap(5);
        break;
    case 6:
        UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"open shooting_range4?listen"), nullptr);
        OnStartMap(6);
        break;
    case 7:
        break;

    default:
        std::cerr << "Invalid map selection" << std::endl;
        break;
    }

    while (true)
    {
        SafeProcessPlayers();

        //Sleep(33.333333333333336); // Frametime @30FPS. Server runs at 30FPS by default, which means 30Hz tickrate. 

        if (GetAsyncKeyState(VK_F1)) {
            UGameplayStatics::FlushLevelStreaming(UWorld::GetWorld());
        }
    }

    return 0;
}

__declspec(dllexport) void __Main() {
    // dummy function for imports   
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        CreateThread(0, 0, (LPTHREAD_START_ROUTINE)MainThread, hModule, 0, 0);
        break;
    }

    return TRUE;
}