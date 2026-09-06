#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <wininet.h>
#pragma comment(lib, "wininet.lib")

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

#include "VehicleList.h"    
#include "SDK/ShadowTrackerExtra_parameters.hpp"

#include "dummy.hpp"

#include "MinHook.h"

#include"cJSON.h"
#include "SDK/Engine_parameters.hpp"

//#include"characterCore.hpp"

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

bool bIsTrainingMode = false;

UWorld* GWorld = nullptr;

enum ECharacterFace : int32_t
{
    Face_01 = 400101,
    Face_02 = 400102,
    Face_03 = 400103,
    Face_04 = 400104,
    Face_05 = 400105,
    Face_06 = 400106,
    Face_07 = 400107,
    Face_08 = 400108
};

enum ECharacterHair : int32_t
{
    // Hair A
    Hair_A_01 = 40601001,
    Hair_A_02 = 40602001,
    Hair_A_03 = 40603001,
    Hair_A_04 = 40604001,
    Hair_A_05 = 40605001,
    Hair_A_06 = 40606001,

    // Hair B
    Hair_B_01 = 40601002,
    Hair_B_02 = 40602002,
    Hair_B_03 = 40603002,
    Hair_B_04 = 40604002,
    Hair_B_05 = 40605002,
    Hair_B_06 = 40606002,

    // Hair C
    Hair_C_01 = 40601003,
    Hair_C_02 = 40602003,
    Hair_C_03 = 40603003,
    Hair_C_04 = 40604003,
    Hair_C_05 = 40605003,
    Hair_C_06 = 40606003,

    // Hair D
    Hair_D_01 = 40601004,
    Hair_D_02 = 40602004,
    Hair_D_03 = 40603004,
    Hair_D_04 = 40604004,
    Hair_D_05 = 40605004,
    Hair_D_06 = 40606004,

    // Hair E
    Hair_E_01 = 40601005,
    Hair_E_02 = 40602005,
    Hair_E_03 = 40603005,
    Hair_E_04 = 40604005,
    Hair_E_05 = 40605005,
    Hair_E_06 = 40606005,

    // Hair F
    Hair_F_01 = 40601006,
    Hair_F_02 = 40602006,
    Hair_F_03 = 40603006,
    Hair_F_04 = 40604006,
    Hair_F_05 = 40605006,
    Hair_F_06 = 40606006,

    // Hair G
    Hair_G_01 = 40601007,
    Hair_G_02 = 40602007,
    Hair_G_03 = 40603007,
    Hair_G_04 = 40604007,
    Hair_G_05 = 40605007,
    Hair_G_06 = 40606007,

    // Hair H
    Hair_H_01 = 40601008,
    Hair_H_02 = 40602008,
    Hair_H_03 = 40603008,
    Hair_H_04 = 40604008,
    Hair_H_05 = 40605008,
    Hair_H_06 = 40606008,

    // Hair I
    Hair_I_01 = 40606009,
    Hair_I_02 = 40601009,
    Hair_I_03 = 40602009,
    Hair_I_04 = 40603009,
    Hair_I_05 = 40604009,
    Hair_I_06 = 40605009
};

#define ACCOUNT_ID_LEN     64

typedef enum
{
    STATUS_EMPTY = 0,
    STATUS_NEEDS_FETCH,  // need to fetch the data from the Node.js API.
    STATUS_FETCHING,     // HTTP in progress on the Tick Thread
    STATUS_NEEDS_APPLY,  // Cached data ready! Game Thread needs to spawn/apply.
    STATUS_READY         // Successfully applied to the figure!
} PlayerStatus;

typedef struct APPEARENCE_
{
    int32_t gender_ID;
    int32_t face_ID;
    int32_t hair_ID;
} Character_appearenceData;

typedef struct CLOTHE_
{
    int32_t head_itemID;
    int32_t mask_itemID;
    int32_t body_itemID;
    int32_t legs_itemID;
    int32_t foot_itemID;

} Character_clothesData;

typedef struct CharacterPlayerData_
{
    char account_id[ACCOUNT_ID_LEN];
    Character_appearenceData appearenceData;
    Character_clothesData clotheData;
    SDK::ASTExtraPlayerController* Controller;
    SDK::ASTExtraBaseCharacter* Character;

    PlayerStatus status;

} CharacterPlayerData;

#define MAX_PLAYERS 100

static CharacterPlayerData g_Players[MAX_PLAYERS];
static CRITICAL_SECTION g_Lock;

SDK::ASTExtraBaseCharacter* GetCharacterFromDController(SDK::APlayerController* PC)
{
    if (!PC) return nullptr;
    if (PC->K2_GetPawn() && PC->K2_GetPawn()->IsA(SDK::ASTExtraBaseCharacter::StaticClass()))
        return static_cast<SDK::ASTExtraBaseCharacter*>(PC->K2_GetPawn());
    return nullptr;
}

int OGBG_UpdateBaseCharacter(SDK::ASTExtraBaseCharacter* STExtraBaseCharacter, SDK::ECharacterGender gender, ECharacterFace head_id, ECharacterHair hair_id)
{
    if (STExtraBaseCharacter)
    {
        SDK::UClass* AvatarCompClass = SDK::UAvatarComponent::StaticClass();

        SDK::UActorComponent* FoundComp = STExtraBaseCharacter->GetComponentByClass(AvatarCompClass);
        if (FoundComp != nullptr)
        {
            SDK::UAvatarComponent* AvatarComponent = reinterpret_cast<SDK::UAvatarComponent*>(FoundComp);
            if (AvatarComponent)
            {
                SDK::UCharacterAvatarComponent* CharacterAvatarComponent = (SDK::UCharacterAvatarComponent*)AvatarComponent;
                if (CharacterAvatarComponent)
                {
                    int32_t updateGender = (int32_t)gender;

                    CharacterAvatarComponent->SetAvatarGender(updateGender); // change main gender of avatar
                    CharacterAvatarComponent->InitialAvatarParam(updateGender); // initialize gender bones
                    CharacterAvatarComponent->InitMasterComponent(updateGender); // align the bones

                    int synCount = CharacterAvatarComponent->synData.Num();
                    for (int i = 0; i < synCount; i++)
                    {
                        CharacterAvatarComponent->synData[i].gender = updateGender;
                    }


                    CharacterAvatarComponent->InitDefaultAvatarByResID(updateGender, head_id, hair_id);

                    // UEMeshReload
                    CharacterAvatarComponent->isNeedRefresh = true;
                    CharacterAvatarComponent->RefreshAvatar();

                    // Unreal network system to report character updates. RPC

                    CharacterAvatarComponent->OnRep_SetDefaultCfg();
                    CharacterAvatarComponent->OnRep_AvatarMeshChanged();

                    return 0;
                }
                else
                {
                    return 1;
                }
                return 0;
            }
            else
            {
                return 1;
            }
        }
        else
        {
            printf("\n OGBG_UpdateBaseCharacter() [ERROR!]:  The character does not actually have an active AvatarComponent.\n");
            return 1;
        }

        return 0;
    }
    else
    {
        return 1;
    }

    return 0;
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

struct FSpeedSample
{
    void* Player;
    FVector LastLocation;

    double LastServerTime;
    float Suspicion;

    bool Used;
};


#define MAX_PLAYERS 100

FSpeedSample SpeedSamples[MAX_PLAYERS];

double GetServerTime()
{
    using namespace std::chrono;

    static auto StartTime = steady_clock::now();

    auto Now = steady_clock::now();

    return duration<double>(Now - StartTime).count();
}

double CalculateDistance(FVector A, FVector B)
{
    double X = A.X - B.X;
    double Y = A.Y - B.Y;
    double Z = A.Z - B.Z;

    return sqrt(X * X + Y * Y + Z * Z);
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

void MH_Initalize2()
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

#define STR(x) #x
#define XSTR(x) STR(x)

#define COSMETIC_API_IP 217.199.220.41
#define COSMETIC_API_PORT 3000

typedef enum
{
    REQ_SUCCESS,   // 200 OK
    REQ_NOT_FOUND, // 404 Not Found
    REQ_CONN_ERROR // Connection Error / Timeout / Server Down
} HttpRequestResult;

// Generic helper function to perform a GET request and capture the status code.
static HttpRequestResult ExecuteHttpGet(const char* urlPath, char** outResponseData)
{
    HINTERNET hInternet = NULL;
    HINTERNET hConnect = NULL;
    HINTERNET hRequest = NULL;
    HttpRequestResult result = REQ_CONN_ERROR;

    // Short timeouts (2 seconds) to avoid blocking the Tick Thread if the API goes down.
    DWORD timeoutMs = 2000;

    hInternet = InternetOpenA("PUBG_Custom_Client/1.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) return REQ_CONN_ERROR;

    // Configures session timeouts
    InternetSetOptionA(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
    InternetSetOptionA(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeoutMs, sizeof(timeoutMs));

    hConnect = InternetConnectA(hInternet, XSTR(COSMETIC_API_IP), COSMETIC_API_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect)
    {
        InternetCloseHandle(hInternet);
        return REQ_CONN_ERROR;
    }

    // ---------------------------------------------------------------------------------
    // CRITICAL: INTERNET_FLAG_RELOAD and INTERNET_FLAG_NO_CACHE_WRITE kill the internal WinINet cache!
    // ---------------------------------------------------------------------------------
    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE;

    hRequest = HttpOpenRequestA(hConnect, "GET", urlPath, NULL, NULL, NULL, flags, 0);
    if (!hRequest)
    {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return REQ_CONN_ERROR;
    }

    if (HttpSendRequestA(hRequest, NULL, 0, NULL, 0))
    {
        // 1. READS THE HTTP STATUS CODE (200, 404, 500, etc.)
        DWORD statusCode = 0;
        DWORD statusCodeLen = sizeof(statusCode);

        HttpQueryInfoA(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
            &statusCode, &statusCodeLen, NULL);

        if (statusCode == 200)
        {
            // Read the response body
            char buffer[1024];
            DWORD bytesRead = 0;
            char* responseData = NULL;
            size_t totalBytes = 0;

            while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0)
            {
                buffer[bytesRead] = '\0';
                char* newMem = (char*)realloc(responseData, totalBytes + bytesRead + 1);
                if (!newMem) { free(responseData); responseData = NULL; break; }
                responseData = newMem;
                memcpy(responseData + totalBytes, buffer, bytesRead);
                totalBytes += bytesRead;
                responseData[totalBytes] = '\0';
            }

            *outResponseData = responseData;
            result = REQ_SUCCESS;
        }
        else if (statusCode == 404)
        {
            result = REQ_NOT_FOUND; // Returned a 404!
        }
        else
        {
            result = REQ_CONN_ERROR; // Another HTTP error (500, 502, etc.)
        }
    }
    else
    {
        result = REQ_CONN_ERROR; // Connection failure (Socket error)
    }

    if (hRequest) InternetCloseHandle(hRequest);
    if (hConnect) InternetCloseHandle(hConnect);
    if (hInternet) InternetCloseHandle(hInternet);

    return result;
}

typedef enum {
    FETCH_OK,          // Found and populated the cJSON
    FETCH_NOT_FOUND,   // Got a 404 on BOTH (Player and Guest).
    FETCH_RETRY_LATER  // A connection error occurred (You should try again in the Tick Thread)
} FetchStatus;

// Parses the cJSONand populates the player struct.
static bool ParseJsonToPlayer(const char* jsonStr, CharacterPlayerData* player)
{
    if (!jsonStr) return false;
    cJSON* json = cJSON_Parse(jsonStr);
    if (!json) return false;

    cJSON* item = cJSON_GetObjectItemCaseSensitive(json, "face_ID");
    if (cJSON_IsNumber(item)) player->appearenceData.face_ID = item->valueint;
    printf("\n ParseJsonToPlayer(): face_ID: %i\n", item->valueint);

    item = cJSON_GetObjectItemCaseSensitive(json, "hair_ID");
    if (cJSON_IsNumber(item)) player->appearenceData.hair_ID = item->valueint;
    printf("\n ParseJsonToPlayer(): hair_ID: %i\n", item->valueint);

    item = cJSON_GetObjectItemCaseSensitive(json, "gender_ID");
    if (cJSON_IsNumber(item)) player->appearenceData.gender_ID = item->valueint;
    printf("\n ParseJsonToPlayer(): gender_ID: %i\n", item->valueint);

    cJSON_Delete(json);
    return true;
}

// 2-LEVEL FALLBACK LOGIC
FetchStatus FetchPlayerAppearanceWithFallback(CharacterPlayerData* player)
{
    char urlPath[256];
    char* jsonResponse = NULL;

    printf("\n FetchPlayerAppearanceWithFallback() Called \n ");

    // -------------------------------------------------------------
    // PASSAGE 1: Registered Player Attempt (/api/player/...)
    // -------------------------------------------------------------
    snprintf(urlPath, sizeof(urlPath), "/api/player/%s/appearance", player->account_id);
    HttpRequestResult res1 = ExecuteHttpGet(urlPath, &jsonResponse);

    if (res1 == REQ_SUCCESS)
    {
        ParseJsonToPlayer(jsonResponse, player);
        free(jsonResponse);
        return FETCH_OK; // Player successfully registered!
    }
    else if (res1 == REQ_CONN_ERROR)
    {
        // If the Node.js API has crashed or timed out, there is no point in testing the Guest.
        // Returns immediately to retry in the next Tick Thread cycle.
        return FETCH_RETRY_LATER;
    }

    // -------------------------------------------------------------
    // STEP 2: If the Player returns a 404, try Guest (/api/guest_player/...)
    // -------------------------------------------------------------
    printf("[FALLBACK 404] %s not found in /player. Trying /guest_player...\n", player->account_id);

    printf("\n Guest_Request_NETID: [%s] \n", player->account_id);

    snprintf(urlPath, sizeof(urlPath), "/api/guest_player/%s/appearance", player->account_id);
    HttpRequestResult res2 = ExecuteHttpGet(urlPath, &jsonResponse);

    if (res2 == REQ_SUCCESS)
    {
        ParseJsonToPlayer(jsonResponse, player);
        free(jsonResponse);
        return FETCH_OK; // Guest Player Success!
    }
    else if (res2 == REQ_CONN_ERROR)
    {
        return FETCH_RETRY_LATER; // Network error on Guest -> Try again later
    }

    // -------------------------------------------------------------
    // PASSAGE 3: Got a 404 on BOTH (/player and /guest_player)
    // -------------------------------------------------------------
    printf("[404 FINAL] %s not registered in ANY database route!\n", player->account_id);
    return FETCH_NOT_FOUND;
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

void CCustomize_System_Init(void)
{
    InitializeCriticalSection(&g_Lock);
    memset(g_Players, 0, sizeof(g_Players));
    g_PendingAppliesCount = 0;
}

bool bot_character_change = 0;

void (*ServerNotifyHitFn)(UTslBallisticsComp*, const struct FServerNotifyHitArgs&) = 0;
void (*ServerNotifyHitSimpleFn)(UTslBallisticsComp*, const struct FServerNotifyHitArgs_Simple&) = 0;
void (*ServerNotifyHitNPCFn)(UTslBallisticsComp*, const struct FServerNotifyHitArgs_NonPlayerCharacter&) = 0;
void* (*ProcessEventO)(UObject* Obj, UFunction* Func, void* Func_Params) = nullptr;

TSubclassOf<class UDamageType> LastDamageType = nullptr;
FAttackId LastAttackId{};

void OnStartMap(int MapId);
//void FixValue();
void Fix_Character_Logic(ASTExtraBaseCharacter* Character);

static int Team1Count = 0;
static int Team2Count = 0;

static int NextTeamID = 1;

//void GenerateAllItems(UItemGeneratorComponent* Generator);

struct FWeaponFireSample
{
    void* Player;

    double LastShotTime;

    int ShotsFired;

    float Suspicion;
};

FWeaponFireSample FireSamples[MAX_PLAYERS];

void CheckFireRate(ASTExtraPlayerController* Player, float WeaponFireDelay)
{
    FWeaponFireSample* Sample = nullptr;

    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (FireSamples[i].Player == Player)
        {
            Sample = &FireSamples[i];
            break;
        }
    }

    if (!Sample)
    {
        for (int i = 0; i < MAX_PLAYERS; i++)
        {
            if (FireSamples[i].Player == nullptr)
            {
                FireSamples[i].Player = Player;
                FireSamples[i].LastShotTime = 0;
                FireSamples[i].Suspicion = 0;

                Sample = &FireSamples[i];
                break;
            }
        }
    }


    if (!Sample)
        return;


    double Now = GetServerTime();


    double TimeSinceLastShot =
        Now - Sample->LastShotTime;


    if (TimeSinceLastShot < WeaponFireDelay)
    {
        Sample->Suspicion += 2;


        printf("Fire rate hack detected\n");
    }
    else
    {
        Sample->Suspicion -= 0.1;


        if (Sample->Suspicion < 0)
            Sample->Suspicion = 0;
    }


    if (Sample->Suspicion > 10)
    {
        Player->STExtraBaseCharacter->Health = 0.0f;
        //PC->KickSelf();
        //PC->ClientLeaveMatchIntentionally();
        Player->ServerLeaveMatchIntentionally();

        ATslLPCPlayerState* ServerPS = (ATslLPCPlayerState*)UGameplayStatics::GetPlayerController(UWorld::GetWorld(), 0)->PlayerState;

        if (ServerPS->IsA(ATslLPCPlayerState::StaticClass()) && Player->GetCurPlayerState()->IsA(ATslLPCPlayerState::StaticClass()))
        {
            ServerPS->BroadcastMidGameBan((ATslLPCPlayerState*)Player->GetCurPlayerState(), FString(L"Banned"), FString(L"Banned"));
        }
        else
        {
            std::cerr << "Condition failed" << std::endl;
        }
    }


    Sample->LastShotTime = Now;
}

void* ProcessEventHook(UObject* Obj, UFunction* Func, void* Func_Params)
{
    if (Obj && Func)
    {
        auto ObjName = Obj->GetName();
        auto FuncName = Func->GetName();

        if (Func->GetName() == "BP_OnWeaponReloadEnd")
        {
            std::cerr << "Reload ended for a player" << std::endl; 

            ASTExtraShootWeapon* shoot = (ASTExtraShootWeapon*)Obj;
            ASTExtraBaseCharacter* wepowner = (ASTExtraBaseCharacter*)shoot->GetOwner();
            ASTExtraPlayerController* wepownerPC = (ASTExtraPlayerController*)wepowner->GetController();

            if (shoot)
            std::cerr << "shoot: " << shoot->GetName() << std::endl;

            if (wepowner)
            std::cerr << "wepowner: " << wepowner->GetName() << std::endl;

            if (wepownerPC)
            std::cerr << "wepownerpc: " << wepownerPC->GetName() << std::endl; 

            int BulletsBefore = 0;
            int BulletsAfter = 0;
            int BulletsToRemove = 0;

            std::cerr << "Bullets before: " << (int)shoot->CurBulletNumInClip << std::endl;
            std::cerr << "Max bullets: " << (int)shoot->CurMaxBulletNumInOneClip << std::endl; 
           // BulletsBefore = (int)shoot->CurBulletNumInClip;

            shoot->CurBulletNumInClip = shoot->CurMaxBulletNumInOneClip;

            std::cerr << "Num changed, current: " << (int)shoot->CurBulletNumInClip << std::endl;
            //shoot->TslBallisticsComp->CurrentAmmoData = shoot->CurMaxBulletNumInOneClip;
            //shoot->TslBallisticsComp->ClientNotifyAmmo(shoot->CurMaxBulletNumInOneClip);
            //std::cerr << "ClientNotifyAmmo called" << std::endl; 
            shoot->StartReload();
            std::cerr << "StartReload called" << std::endl; 
            shoot->SimulateWeaponReload(EWeaponReloadAnimExec::Tactical, shoot->CurMaxBulletNumInOneClip);
            std::cerr << "SimulateWeaponReload called" << std::endl; 
            shoot->SetCurrentBulletNumInClipOnClient(shoot->CurMaxBulletNumInOneClip);
            std::cerr << "SetCurrentBulletNumInClipOnClient called" << std::endl; 
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

            /*
            UTslBallisticsComp* Wep = (UTslBallisticsComp*)Obj;
            auto ShootWeapon = (ASTExtraShootWeapon*)Wep->GetOwner();
            ASTExtraBaseCharacter* Character = (ASTExtraBaseCharacter*)ShootWeapon->GetOwner();
            std::cerr << "OWNER: " << ShootWeapon->GetOwner()->GetName() << std::endl; 
            std::cerr << "Time between shots: " << Wep->WeaponGunConfig->WeaponGunConfig.TimeBetweenShots << std::endl; 
            */

            //CheckFireRate(Character->GetPlayerControllerSafety(), Wep->WeaponGunConfig->WeaponGunConfig.TimeBetweenShots);

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

                    //std::cerr << "Owner: " << weaponowner->GetName() << std::endl;
                    //std::cerr << "Hit actor: " << ActorHit->GetName() << std::endl;

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

                    /*
                     if (OwnerChar->Health <= 0 || OwnerChar->bDead == true)
                     {
                         std::cerr << "Owner is dead, skipping damage..." << std::endl;
                         return ProcessEventO(Obj, Func, Func_Params);
                     }
                     */

                    //ASTExtraBaseCharacter* basechar = (ASTExtraBaseCharacter*)Args.ServerOriginImpact.Actor.Get();
                    //ASTExtraGameStateBase* gamestate = (ASTExtraGameStateBase*)UGameplayStatics::GetGameState(UWorld::GetWorld());

                    /*
                    SDK::USTExtraGameplayStatics::STApplyPointDamage(Args.OptimizedHitResult.HitActor.Get(),
                        finalDamage,
                        Args.ClientOriginAmmoStartLoc,
                        Final,
                        weaponowner->GetOwnerController(),
                        weaponowner,
                        weaponowner->ShootWeaponComponent->ShootWeaponEntityComponent->DamageType);
                        */

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

        if (Func->GetName() == "FlushServerNotifyHitList")
        {
            std::cerr << "FlushServerNotifyHitList CALLED" << std::endl;

            auto Params = (Params::TslBallisticsComp_FlushServerNotifyHitList*)Func_Params;

            /*
            UTslBallisticsComp* Wep = (UTslBallisticsComp*)Obj;
            auto ShootWeapon = (ASTExtraShootWeapon*)Wep->GetOwner();
            ASTExtraBaseCharacter* Character = (ASTExtraBaseCharacter*)ShootWeapon->GetOwner();
            std::cerr << "OWNER: " << ShootWeapon->GetOwner()->GetName() << std::endl;
            std::cerr << "Time between shots: " << Wep->WeaponGunConfig->WeaponGunConfig.TimeBetweenShots << std::endl;
            */

            //CheckFireRate(Character->GetPlayerControllerSafety(), Wep->WeaponGunConfig->WeaponGunConfig.TimeBetweenShots);

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
                            /*
        TWeakObjectPtr<class AActor>                  HitActor;                                          // 0x0000(0x0008)(ZeroConstructor, IsPlainOldData, NoDestructor, UObjectWrapper, HasGetValueTypeHash, NativeAccessSpecifierPublic)
        class FName                                   BoneName;                                          // 0x0008(0x0008)(ZeroConstructor, IsPlainOldData, NoDestructor, HasGetValueTypeHash, NativeAccessSpecifierPublic)
        struct FVector_NetQuantize                    Location;                                          // 0x0010(0x000C)(NoDestructor, NativeAccessSpecifierPublic)
        struct FVector_NetQuantize                    ImpactPoint;                                       // 0x001C(0x000C)(NoDestructor, NativeAccessSpecifierPublic)
        TWeakObjectPtr<class UPhysicalMaterial>       PhysMaterial;                                      // 0x0028(0x0008)(ZeroConstructor, IsPlainOldData, NoDestructor, UObjectWrapper, HasGetValueTypeHash, NativeAccessSpecifierPublic)
        TWeakObjectPtr<class UPrimitiveComponent>     Component;                                         // 0x0030(0x0008)(ExportObject, ZeroConstructor, InstancedReference, IsPlainOldData, NoDestructor, UObjectWrapper, HasGetValueTypeHash, NativeAccessSpecifierPublic)
        struct FVector_NetQuantizeNormal              ImpactNormal;
                            */

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

                            /*
                             if (OwnerChar->Health <= 0 || OwnerChar->bDead == true)
                             {
                                 std::cerr << "Owner is dead, skipping damage..." << std::endl;
                                 return ProcessEventO(Obj, Func, Func_Params);
                             }
                             */

                            ASTExtraBaseCharacter* basechar = (ASTExtraBaseCharacter*)Args.OptimizedHitResult.HitActor.Get();
                            //ASTExtraGameStateBase* gamestate = (ASTExtraGameStateBase*)UGameplayStatics::GetGameState(UWorld::GetWorld());
                            if (basechar->Health - finalDamage <= 0)
                            {
                                std::cerr << "Fatal damage detected" << std::endl;
                                //OwnerChar->TryToBroadcastFatalDamageEvent(OwnerChar, )
                                //OwnerChar->BroadcastFatalDamageInfo(OwnerChar, basechar, Args.AmmoId, 0, bIsHeadShot, basechar->Health - finalDamage, basechar->Health, OwnerChar, ownerPS->Kills);
                                //return ProcessEventO(Obj, Func, Func_Params);
                            }

                            /*
                            SDK::USTExtraGameplayStatics::STApplyPointDamage(Args.OptimizedHitResult.HitActor.Get(),
                                finalDamage,
                                Args.ClientOriginAmmoStartLoc,
                                Final,
                                weaponowner->GetOwnerController(),
                                weaponowner,
                                weaponowner->ShootWeaponComponent->ShootWeaponEntityComponent->DamageType);
                                */

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

void Fix_Character_Logic(ASTExtraBaseCharacter* Character)
{
    if (!Character)
    {
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

struct FCharacterDamageTickState
{
    int32 LastAmmo = -1;
    int32 LastClientShootTimes = -1;
    int32 LastHitDataCount = -1;
    uint32 LastProcessedHitShootID = 0;
    uint32 LastNativeUploadShootID = 0;
    uint32 LastSyntheticUploadShootID = 0;
    DWORD LastGunDamageTick = 0;
    DWORD LastMeleeDamageTick = 0;
    DWORD LastNoTargetLogTick = 0;
};

static std::unordered_map<void*, FCharacterDamageTickState> CharacterStates;

int32 GetSafeHitDataCount(SDK::ASTExtraShootWeapon* Weapon)
{
    if (!Weapon)
    {
        return 0;
    }

    const int32 Count = Weapon->HitDataArray.Num();
    /*
    if (Count < 0 || Count > 64)
    {
        return 0;
    }
    */

    return Count;
}

uint32 GetLatestHitShootID(SDK::ASTExtraShootWeapon* Weapon)
{
    const int32 Count = GetSafeHitDataCount(Weapon);
    if (Count <= 0)
    {
        return 0;
    }

    return Weapon->HitDataArray[Count - 1].ShootID;
}

void UpdateWeaponSnapshot(FCharacterDamageTickState& State, SDK::ASTExtraShootWeapon* Weapon)
{
    if (!Weapon)
    {
        return;
    }

    State.LastAmmo = Weapon->CurBulletNumInClip;
    State.LastClientShootTimes = Weapon->ClientShootTimes;
    State.LastHitDataCount = GetSafeHitDataCount(Weapon);
}

DWORD GetGunDamageIntervalMs(SDK::ASTExtraShootWeapon* Weapon)
{
    if (Weapon && Weapon->TslBallisticsComp)
    {
        const float Seconds = Weapon->TslBallisticsComp->GetTimeBetweenShots();
        if (Seconds > 0.03f && Seconds < 1.0f)
        {
            return static_cast<DWORD>(std::clamp(Seconds * 1000.0f, 60.0f, 350.0f));
        }
    }

    return 120;
}

void ResetWeaponSnapshot(FCharacterDamageTickState& State, SDK::ASTExtraShootWeapon* Weapon)
{
    UpdateWeaponSnapshot(State, Weapon);
    State.LastProcessedHitShootID = GetLatestHitShootID(Weapon);
}

bool IsCharacterReloading(SDK::ASTExtraPlayerCharacter* Character)
{
    if (!Character)
    {
        return false;
    }

    if (Character->CurrentReloadWeapon)
    {
        return true;
    }

    auto It = ReloadingPlayers.find(Character);
    return It != ReloadingPlayers.end() && It->second.bWaitingForReload;
}

bool CheckIfActorActive(SDK::UWorld* World, SDK::AActor* TargetActor)
{
    if (!World || !TargetActor) return false;

    for (int i = UObject::GObjects->Num() - 1; i >= 0; --i)
    {
        UObject* Obj = UObject::GObjects->GetByIndex(i);

        if (!Obj || Obj->IsDefaultObject())
            continue;

        if (Obj->IsA(AActor::StaticClass()))
        {
            AActor* Actor = (AActor*)Obj;

            if (Actor == TargetActor)
            {
                return true;
            }
        }
    }

    return false;
}

struct FActorSpawnParameters
{
    SDK::FName Name;                                // 0x00
    SDK::AActor* Template;                          // 0x08
    SDK::AActor* Owner;                             // 0x10
    SDK::APawn* Instigator;                         // 0x18
    SDK::ULevel* OverrideLevel;                     // 0x20
    SDK::ESpawnActorCollisionHandlingMethod SpawnCollisionHandlingOverride;   // 0x28 
    uint8_t Pad_29[0x1];                            // 0x29
    uint16_t bRemoteOwned : 1;                      // 0x2A.0
    uint16_t bNoFail : 1;                           // 0x2A.1
    uint16_t bDeferConstruction : 1;                // 0x2A.2
    uint16_t bAllowDuringConstructionScript : 1;    // 0x2A.3
    uint16_t : 12;                                  // 0x2A padding
    SDK::EObjectFlags ObjectFlags;                  // 0x2C
};

FActorSpawnParameters Construct_ActorSpawnParameters()
{
    FActorSpawnParameters params = { 0 };

    params.Name = SDK::FName();
    params.Template = nullptr;
    params.Owner = nullptr;
    params.Instigator = nullptr;
    params.OverrideLevel = nullptr;

    params.SpawnCollisionHandlingOverride = SDK::ESpawnActorCollisionHandlingMethod::Undefined;

    params.bNoFail = true;
    params.bDeferConstruction = false;
    params.bRemoteOwned = false;
    params.bAllowDuringConstructionScript = false;

    params.ObjectFlags = SDK::EObjectFlags::Transactional;


    return params;
}

SDK::AActor* SpawnActor(SDK::UWorld* world,
    SDK::UClass* actorClass,
    SDK::FVector* location,
    FActorSpawnParameters* params)
{
    if (!world)
    {
        printf("\n SpawnActor: World Is NULL! \n");
    }
    if (!actorClass)
    {
        printf("\n SpawnActor: actorClass is NULL! \n");
    }
    if (!location)
    {
        printf("\n SpawnActor: location is NULL! \n");
    }
    if (!params)
    {
        printf("\n SpawnActor: params is NULL! \n");
    }

    typedef SDK::AActor* (__fastcall* Fn)(SDK::UWorld*,
        SDK::UClass*,
        SDK::FVector*,
        FActorSpawnParameters*);

    static Fn fn = (Fn)((uintptr_t)GetModuleHandle(NULL) + 0xF40AB0);

    return fn(world, actorClass, location, params);
}

void* UWorld_SpawnActor_proxy(SDK::UWorld* self, SDK::UClass* ActorClass,
    SDK::FVector* location, SDK::FRotator* rotation, FActorSpawnParameters* ActorSpawnParams)
{
    typedef void* (__fastcall* FnSpawnActor)(SDK::UWorld*,
        SDK::UClass*,
        SDK::FVector*,
        SDK::FRotator*,
        FActorSpawnParameters*);

    static FnSpawnActor fn = (FnSpawnActor)((uintptr_t)GetModuleHandle(NULL) + 0xF41C10);

    return fn(self, ActorClass, location, rotation, ActorSpawnParams);
}

static FQuat RotatorToQuat(const FRotator& Rot)
{
    const float DEG_TO_RAD = 3.14159265358979323846f / 180.0f;

    const float SP = sinf(Rot.Pitch * DEG_TO_RAD * 0.5f);
    const float CP = cosf(Rot.Pitch * DEG_TO_RAD * 0.5f);

    const float SY = sinf(Rot.Yaw * DEG_TO_RAD * 0.5f);
    const float CY = cosf(Rot.Yaw * DEG_TO_RAD * 0.5f);

    const float SR = sinf(Rot.Roll * DEG_TO_RAD * 0.5f);
    const float CR = cosf(Rot.Roll * DEG_TO_RAD * 0.5f);

    FQuat Q;
    Q.X = CR * SP * SY - SR * CP * CY;
    Q.Y = -CR * SP * CY - SR * CP * SY;
    Q.Z = CR * CP * SY - SR * SP * CY;
    Q.W = CR * CP * CY + SR * SP * SY;

    return Q;
}

void FixHitBox(SDK::UPrimitiveComponent* HitBox)
{
    if (!HitBox) return;

    if (HitBox)
    {
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

void CheckMovement(ASTExtraPlayerController* PC, APawn* Pawn);

void ProcessPlayers()
{
    SDK::UWorld* World = GWorld;

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
            /*
            ANewFakePlayerAIController* AIController = (ANewFakePlayerAIController*)STEPC;

            if (AIController->ControlledCharacter->GetHitBoxByState(EPawnState::Stand)->GetCollisionEnabled() != ECollisionEnabled::QueryAndPhysics)
            {
                AIController->ControlledCharacter->GetHitBoxByState(EPawnState::Stand)->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            }

            if (AIController->ControlledCharacter->GetHitBoxByState(EPawnState::Crouch)->GetCollisionEnabled() != ECollisionEnabled::QueryAndPhysics)
            {
                AIController->ControlledCharacter->GetHitBoxByState(EPawnState::Crouch)->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            }

            if (AIController->ControlledCharacter->GetHitBoxByState(EPawnState::GunFire)->GetCollisionEnabled() != ECollisionEnabled::QueryAndPhysics)
            {
                AIController->ControlledCharacter->GetHitBoxByState(EPawnState::GunFire)->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            }

            if (AIController->ControlledCharacter->GetHitBoxByState(EPawnState::GunReload)->GetCollisionEnabled() != ECollisionEnabled::QueryAndPhysics)
            {
                AIController->ControlledCharacter->GetHitBoxByState(EPawnState::GunReload)->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            }

            if (AIController->ControlledCharacter->GetHitBoxByState(EPawnState::GunADS)->GetCollisionEnabled() != ECollisionEnabled::QueryAndPhysics)
            {
                AIController->ControlledCharacter->GetHitBoxByState(EPawnState::GunADS)->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            }
            */

            continue;
        }

        ASTExtraBaseCharacter* Character = STEPC->STExtraBaseCharacter; // this make crash if STEPC == NULL pointer
        if (!Character)
            continue;

        CheckMovement(STEPC, Character);

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

void SpawnVehicles_Erangel()
{
    UWorld* World = UWorld::GetWorld();

    /*
    450812, 314647, 552.324
    425945, 338953, 438.929
    414679, 345131, 247.185
    465023, 316652, 845.303
    444228, 300254, 343.036
    409765, 296691, 566.13
    */

    FTransform Pos1{};
    Pos1.Translation = FVector(450812, 314647, 552.324);
    Pos1.Rotation = FQuat(1, 1, 1);
    Pos1.Scale3D = FVector(1, 1, 1);

    FTransform Pos2{};
    Pos1.Translation = FVector(425945, 338953, 438.929);
    Pos1.Rotation = FQuat(1, 1, 1);
    Pos1.Scale3D = FVector(1, 1, 1);

    FTransform Pos3{};
    Pos1.Translation = FVector(414679, 345131, 247.185);
    Pos1.Rotation = FQuat(1, 1, 1);
    Pos1.Scale3D = FVector(1, 1, 1);

    FTransform Pos4{};
    Pos1.Translation = FVector(465023, 316652, 845.303);
    Pos1.Rotation = FQuat(1, 1, 1);
    Pos1.Scale3D = FVector(1, 1, 1);

    FTransform Pos5{};
    Pos1.Translation = FVector(444228, 300254, 343.036);
    Pos1.Rotation = FQuat(1, 1, 1);
    Pos1.Scale3D = FVector(1, 1, 1);

    FTransform Pos6{};
    Pos1.Translation = FVector(409765, 296691, 566.13);
    Pos1.Rotation = FQuat(1, 1, 1);
    Pos1.Scale3D = FVector(1, 1, 1);

    auto UAZ_Open = UObject::FindObjectFast(Vehicles::UAZ::Open);
    auto Buggy = UObject::FindObjectFast(Vehicles::Buggy::ForestCamo);
    auto Dacia = UObject::FindObjectFast(Vehicles::Dacia::Blue);
    auto Motorcycle = UObject::FindObjectFast(Vehicles::Motorcycle::Brown);

    SpawnActorFromClass(World, UAZ_Open->GetClass(), Pos1, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn, nullptr);
    std::cerr << "Spawned Vehicle" << std::endl;
    SpawnActorFromClass(World, Buggy->GetClass(), Pos2, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn, nullptr);
    std::cerr << "Spawned Vehicle" << std::endl;
    SpawnActorFromClass(World, Dacia->GetClass(), Pos3, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn, nullptr);
    std::cerr << "Spawned Vehicle" << std::endl;
    SpawnActorFromClass(World, Motorcycle->GetClass(), Pos4, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn, nullptr);
    std::cerr << "Spawned Vehicle" << std::endl;
    SpawnActorFromClass(World, UAZ_Open->GetClass(), Pos5, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn, nullptr);
    std::cerr << "Spawned Vehicle" << std::endl;
    SpawnActorFromClass(World, Buggy->GetClass(), Pos6, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn, nullptr);
    std::cerr << "Spawned Vehicle" << std::endl;

}

void DisableLevelStreaming()
{
    UWorld* World = UWorld::GetWorld();

    for (ULevelStreaming* StreamingLevelKismet1 : World->StreamingLevels)
    {
        std::cerr << "Iterating through level: " << StreamingLevelKismet1->GetName() << std::endl;
        //std::cerr << "Default LOD index: " << StreamingLevel->LevelLODIndex << std::endl;

        ULevelStreamingKismet* StreamingLevelKismet = (ULevelStreamingKismet*)StreamingLevelKismet1;
        StreamingLevelKismet->bInitiallyLoaded = true;
        StreamingLevelKismet->bInitiallyVisible = true;

        /*
        StreamingLevel->bLocked = false;
        StreamingLevel->bShouldBeLoaded = true;
        StreamingLevel->bShouldBeVisible = true;
        StreamingLevel->bDisableDistanceStreaming = true;
        StreamingLevel->bShouldBlockOnLoad = true;
        */
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

    UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), FString(L"LoadAllLand 1"), nullptr);
    //UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), FString(L"LocalSpawnAI 13"), nullptr);

    /*
    for (int i = 0; i < UObject::GObjects->Num(); ++i)
    {
        UObject* Obj = UObject::GObjects->GetByIndex(i);

        if (!Obj || Obj->IsDefaultObject())
            continue;

        if (Obj->IsA(APUBGDoor::StaticClass()))
        {
            APUBGDoor* Door = (APUBGDoor*)Obj;
            HookProcessEventForCharacter(Door);
        }
    }
    */

    /*
    for (int i = 0; i < UObject::GObjects->Num(); ++i)
    {
        UObject* Obj = UObject::GObjects->GetByIndex(i);

        if (!Obj || Obj->IsDefaultObject())
            continue;

        if (Obj->IsA(URendererSettings::StaticClass()))
        {
            std::cerr << "RendererSettings found: " << Obj->GetName() << std::endl; 
            URendererSettings* RendererSettings = (URendererSettings*)Obj;

            std::cerr << "VTF Landscape: " << (int)RendererSettings->bMobileVTFLandscape << std::endl; 
            std::cerr << "bDiscardUnusedQualityLevels: " << (int)RendererSettings->bDiscardUnusedQualityLevels << std::endl; 

            RendererSettings->bOcclusionCulling = false;
            RendererSettings->bMobileVTFLandscape = true;
            RendererSettings->bDiscardUnusedQualityLevels = false;
            RendererSettings->bTextureStreaming = false;
            RendererSettings->bCompressMeshDistanceFields = false;
        }

        if (Obj->IsA(APrecomputedVisibilityOverrideVolume::StaticClass()))
            std::cerr << "APrecomputedVisibilityOverrideVolume: " << Obj->GetName() << std::endl;

        if (Obj->IsA(UPrimitiveComponent::StaticClass()))
        {
            UPrimitiveComponent* Component = (UPrimitiveComponent*)Obj;

            Component->bAllowCullDistanceVolume = false;
            Component->bBoundsChangeTriggersStreamingDataRebuild = false;
            Component->bForceMipStreaming = false;
        }

        if (Obj->IsA(ALevelStreamingVolume::StaticClass()))
        {
            ALevelStreamingVolume* LevelStreamingVolume = (ALevelStreamingVolume*)Obj;
            LevelStreamingVolume->bDisabled = true;
            LevelStreamingVolume->StreamingUsage = EStreamingVolumeUsage::SVB_VisibilityBlockingOnLoad;
        }

        if (Obj->IsA(UStaticMeshComponent::StaticClass()))
        {
            UStaticMeshComponent* Mesh = (UStaticMeshComponent*)Obj;

            Mesh->bForceMipStreaming = false;
            Mesh->bIgnoreInstanceForTextureStreaming = true;
            Mesh->StreamingDistanceMultiplier = 1000.f;
            Mesh->SetBoundsScale(1000.f);
        }

        if (Obj->IsA(URuntimeMeshComponent::StaticClass()))
        {
            std::cerr << "RuntimeMeshComponent: " << Obj->GetName() << std::endl;
            
            URuntimeMeshComponent* MeshComp = (URuntimeMeshComponent*)Obj;

            MeshComp->LDMaxDrawDistance = FLT_MAX;
            MeshComp->SetCullDistance(FLT_MAX);
            MeshComp->MinDrawDistance = FLT_MAX;
            MeshComp->bAllowCullDistanceVolume = false;
            MeshComp->SetBoundsScale(1000.f);
        }

        if (Obj->IsA(UStreamingSettings::StaticClass()))
        {
            std::cerr << "StreamingSettings: " << Obj->GetName() << std::endl;

            UStreamingSettings* StreamingSettings = (UStreamingSettings*)Obj;

            StreamingSettings->UseBackgroundLevelStreaming = false;
        }

        if (Obj->IsA(AMyLandscape::StaticClass()))
        {
            std::cerr << "MyLandscape: " << Obj->GetName() << std::endl;

            AMyLandscape* MyLandscape = (AMyLandscape*)Obj;

            std::cerr << "Landscape RuntimeMesh: " << MyLandscape->Mesh->GetName() << std::endl;
            std::cerr << "Landscape Geometry: " << MyLandscape->LandscapeGeometry->GetName() << std::endl;

            for (ULevelStreaming* Level : MyLandscape->LevelStreamings)
            {
                std::cerr << "Landscape level: " << Level->GetName();
            }
        }
        
        if (Obj->IsA(ALandscapeStreamingProxy::StaticClass()))
        {
            std::cerr << "ALandscapeStreamingProxy: " << Obj->GetName() << std::endl;

            ALandscapeStreamingProxy* StreamingProxy = (ALandscapeStreamingProxy*)Obj;

            StreamingProxy->StreamingDistanceMultiplier = 100000.0f;
            StreamingProxy->MaxLODLevel = 0;

            StreamingProxy->SetActorHiddenInGame(false);
            StreamingProxy->SetActorEnableCollision(true);
        }

        if (Obj->IsA(ALandscapeProxy::StaticClass()))
        {
            ALandscapeProxy* StreamingProxy = (ALandscapeProxy*)Obj;

            StreamingProxy->StreamingDistanceMultiplier = FLT_MAX;
            StreamingProxy->MaxLODLevel = 0;

            StreamingProxy->SetActorHiddenInGame(false);
            //StreamingProxy->ChangeLODDistanceFactor(0.f);
            StreamingProxy->MaxLODLevel = 0;
        }
        
        if (Obj->IsA(ULandscapeComponent::StaticClass()))
        {
            ULandscapeComponent* LandscapeComponent = (ULandscapeComponent*)Obj;

            LandscapeComponent->SetBoundsScale(FLT_MAX);

            LandscapeComponent->SetVisibility(true, false);
            LandscapeComponent->SetHiddenInGame(false, false);

            LandscapeComponent->bForceMipStreaming = false;
            LandscapeComponent->ForcedLOD = false;
            LandscapeComponent->LODBias = 0;

            LandscapeComponent->CachedLocalBox.Min = FVector(-10000000.f);
            LandscapeComponent->CachedLocalBox.Max = FVector(10000000.f);

            LandscapeComponent->CollisionComponent->CachedMaxDrawDistance = FLT_MAX;
            LandscapeComponent->CollisionComponent->LDMaxDrawDistance = FLT_MAX;

            LandscapeComponent->CollisionComponent->CachedLocalBox.Min = FVector(-10000000.f);
            LandscapeComponent->CollisionComponent->CachedLocalBox.Max = FVector(10000000.f);
        }
    }
    */

    UEngine::GetEngine()->StreamingDistanceFactor = 1000.0f;

    //std::cerr << "Running FlushLevelStreaming..." << std::endl;
    //UGameplayStatics::FlushLevelStreaming(UWorld::GetWorld());

    /*
    for (ULevelStreaming* streaming : UWorld::GetWorld()->WorldComposition->TilesStreaming)
    {
        printf("%s AFTER FLUSHED LEVEL STREAMING Loaded=%s\n",
            streaming->PackageNameToLoad.ToString().c_str(),
            streaming->LoadedLevel->GetName().c_str());

        auto LS = streaming;

        printf("Package: %s\n", LS->PackageNameToLoad.ToString());
        printf("ShouldLoad %d Visible %d\n",
            LS->bShouldBeLoaded,
            LS->bShouldBeVisible);

        printf("Loaded %p\n", LS->LoadedLevel);

        printf("LODIndex %d\n", LS->LevelLODIndex);

        for (auto& lod : LS->LODPackageNames)
        {
            printf("LOD package %s\n", lod.ToString());
        }
    }
    */

    /*
    std::cerr << "WorldComposition + 0x40 = " << (int)UWorld::GetWorld()->WorldComposition->Pad_28[0x18];
    UWorld::GetWorld()->WorldComposition->Pad_28[0x18] = 0;
    std::cerr << "WorldComposition + 0x40 AFTER = " << (int)UWorld::GetWorld()->WorldComposition->Pad_28[0x18];
    */

    //StaticDoorFix();
    //StaticItemGeneratorFix();
    //ActorLoop();
}

void FixValue();

enum ENetMode
{
    NM_Standalone = 0,
    NM_DedicatedServer = 1,
    NM_ListenServer = 2,
    NM_Client = 3
};

__int64 (*GetNetModeO)(SDK::UObject* a1);
__int64 GetNetModeHook(SDK::UObject* a1)
{
    //std::cout << "GetNetMode called BY: " << a1->GetFullName() << std::endl;
    return ENetMode::NM_DedicatedServer;
}

struct FViewInfo
{
    uint8_t data[1];
};

uint64_t GetPtr(FViewInfo* View, int offset)
{
    return *(uint64_t*)((uintptr_t)View + offset);
}

int GetInt(FViewInfo* View, int offset)
{
    return *(int*)((uintptr_t)View + offset);
}

void MakeEverythingVisible(FViewInfo* View)
{
    int Count = *(int*)((uintptr_t)View + 4912);

    uint64_t Data =
        *(uint64_t*)((uintptr_t)View + 4904);

    uint32_t* Bits;

    if (Data)
        Bits = (uint32_t*)Data;
    else
        Bits = (uint32_t*)((uintptr_t)View + 4888);


    int Words = (Count + 31) / 32;

    for (int i = 0; i < Words; i++)
        Bits[i] = 0xFFFFFFFF;
}

typedef void(*tSub4CDD80)(
    uint64_t RHICmdList,
    uint64_t Scene,
    FViewInfo* View
    );

tSub4CDD80 oSub4CDD80;


void hkSub4CDD80(
    uint64_t RHICmdList,
    uint64_t Scene,
    FViewInfo* View)
{
    MakeEverythingVisible(View);

    //oSub4CDD80(RHICmdList, Scene, View);
}

typedef void(*tSub4D3140)(uint64_t Scene, FViewInfo* View);

tSub4D3140 oSub4D3140;

void hkSub4D3140(uint64_t Scene, FViewInfo* View)
{
    MakeEverythingVisible(View);

    //oSub4D3140(Scene, View);
}

typedef __int64(*tGather)(
    uint64_t,
    uint64_t,
    uint64_t,
    uint64_t,
    uint64_t,
    uint32_t,
    uint64_t
    );

tGather oGather;


__int64 hkGather(
    uint64_t a1,
    uint64_t a2,
    uint64_t Scene,
    uint64_t a4,
    uint64_t Visibility,
    uint32_t Flags,
    uint64_t a7)
{
    return oGather(
        a1,
        a2,
        Scene,
        a4,
        Visibility,
        Flags,
        a7
    );
}

void OnStartMap(int MapId)
{
    GMapId = MapId;
    //Sleep(5500);

    do
    {
        Sleep(20);
    } while (!UWorld::GetWorld());

    GWorld = UWorld::GetWorld();

    std::cerr << "World: " << GWorld->GetName() << std::endl;

    Sleep(4000);
    //UWorld::GetWorld()->WorldComposition->Pad_28[0x71] = 1;

    //ASTExtraBaseCharacter* ServerPawn = (ASTExtraBaseCharacter*)UGameplayStatics::GetPlayerCharacter(UWorld::GetWorld(), 0);
    //ASTExtraPlayerController* ServerPC = (ASTExtraPlayerController*)UGameplayStatics::GetPlayerController(UWorld::GetWorld(), 0);

    //ServerPC->LoadAllLand(true);

    std::cerr << "World loaded." << std::endl;

    //DisableLevelStreaming();

    //std::cerr << "SeamlessTravel: " << (int)UGameplayStatics::GetGameMode(UWorld::GetWorld())->bUseSeamlessTravel << std::endl;
    //UGameplayStatics::GetGameMode(UWorld::GetWorld())->bUseSeamlessTravel = false;

    //UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), FString(L"LoadAllLand 1"), nullptr); 

    printf("\x54""H\111S\x20""I\123 \x46""R\105E\x20""C\117N\x54""E\116T\x20""B\131 \x4F""G\072B\x41""T\124L\x45""G\122O\x55""N\104S\x2C"" \111F\x20""Y\117U\x20""B\117U\x47""H\124 \x54""H\111S\x2C"" \131O\x55"" \110A\x56""E\040B\x45""E\116 \x53""C\101M\x4D""E\104.\x20""h\164t\x70""s\072/\x2F""g\151t\x68""u\142.\x63""o\155/\x48""4\124I\x55""X\012");

    //HookProcessEventForObject(UGameplayStatics::GetPlayerController(UWorld::GetWorld(), 0));


    if (MapId != 6)
    {
        /*
        std::cerr << "Waiting for Character..." << std::endl;
        while (!UGameplayStatics::GetPlayerCharacter(UWorld::GetWorld(), 0))
        {
            Sleep(100);
        }
        */

        if (UGameplayStatics::GetGameMode(UWorld::GetWorld())->IsA(ATeamMatchGameMode::StaticClass()) && UGameplayStatics::GetCurrentLevelName(UWorld::GetWorld(), 1).ToString().contains("Bodie"))
        {
            std::cerr << "Bodie" << std::endl;

            ATeamMatchGameMode* GM = (ATeamMatchGameMode*)UGameplayStatics::GetGameMode(UWorld::GetWorld());
            ATeamMatchGameState* GS = (ATeamMatchGameState*)GM->GameState;

            GM->TeamSize = 5;
            GS->NumTeams = 2;
        }
    }
    
    /*
    UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"r.StaticMeshStreaming 0"), UGameplayStatics::GetPlayerController(UWorld::GetWorld(), 0));
    UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"r.StreamingSM.NeverStreamOut 1"), UGameplayStatics::GetPlayerController(UWorld::GetWorld(), 0));
    UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"r.StaticMeshLODDistanceScale 0"), UGameplayStatics::GetPlayerController(UWorld::GetWorld(), 0));
    UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"r.MipMapLODBias 0"), UGameplayStatics::GetPlayerController(UWorld::GetWorld(), 0));
    UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"r.VirtualTexture 0"), UGameplayStatics::GetPlayerController(UWorld::GetWorld(), 0));
    UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"r.DistanceCullingFactor 0"), UGameplayStatics::GetPlayerController(UWorld::GetWorld(), 0));
    UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"r.DisableLODFade 1"), UGameplayStatics::GetPlayerController(UWorld::GetWorld(), 0));
    UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"r.AOViewFadeDistanceScale 99999999"), UGameplayStatics::GetPlayerController(UWorld::GetWorld(), 0));
    UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"r.LandscapeLODBias 0"), UGameplayStatics::GetPlayerController(UWorld::GetWorld(), 0));
    */
    
    /*
    for (ULevelStreaming* Tile : UWorld::GetWorld()->WorldComposition->TilesStreaming)
    {
        Tile->bShouldBeLoaded = true;
        Tile->bShouldBeVisible = true;
        Tile->bDisableDistanceStreaming = true;
    }
    */

    //ASTExtraBaseCharacter* Char = (ASTExtraBaseCharacter*)UGameplayStatics::GetPlayerCharacter(UWorld::GetWorld(), 0);

    /*
    Char->GetPlayerControllerSafety()->GetCurPlayerState()->bIsSpectator = true;
    Char->GetPlayerControllerSafety()->GetCurPlayerState()->bOnlySpectator = true;

    Char->GetPlayerControllerSafety()->bHidden = true;
    Char->GetPlayerControllerSafety()->bIsSpectating = true;
    Char->GetPlayerControllerSafety()->ChangeSpectatorStateToFreeView();
    */

    /*
    AActor* PlayerViewTarget = STEPC->GetViewTarget();
    UGameplayStatics::GetPlayerController(World, 0)->SetViewTargetWithBlend(STEPC->GetViewTarget(), 0.0f, EViewTargetBlendFunction::VTBlend_Linear,
        0.0f, true);
        */

    //ServerPC->EnableCheats();
    //ServerPC->EnableMyLandscapeDraw();
    //ServerPC->LoadAllLand(true);
    //std::cerr << "bLoadAll: " << (int)UWorld::GetWorld()->WorldComposition->Pad_28[0x71] << std::endl; 

    //ASTExtraGameStateBase* GameStateBase = (ASTExtraGameStateBase*)UGameplayStatics::GetGameState(UWorld::GetWorld());
    //GameStateBase->bGunSamePriority = 1;
    //GameStateBase->bDebugEnableDamageEffectInTrainingMode = true;
    //GameStateBase->bIsTrainingMode = false;

    /*
    for (int i = 0; i < UObject::GObjects->Num(); ++i)
    {
        UObject* Obj = UObject::GObjects->GetByIndex(i);

        if (!Obj || Obj->IsDefaultObject())
            continue;
        
        if (Obj->IsA(SDK::URuntimeMeshComponent::StaticClass()))
        {
            SDK::URuntimeMeshComponent* RunMesh = (SDK::URuntimeMeshComponent*)Obj;

            if (!RunMesh)
                continue;

            std::cerr << "MESH: " << RunMesh->GetName() << " COLLISION: " << (int)RunMesh->GetCollisionEnabled() << std::endl;

            RunMesh->LocalBounds.Origin = FVector(0, 0, 0);
            RunMesh->LocalBounds.BoxExtent = FVector(1000000, 1000000, 1000000);
            RunMesh->LocalBounds.SphereRadius = 2000000;

            RunMesh->SetVisibility(true, false);
            RunMesh->SetHiddenInGame(false, false);

            RunMesh->SetComponentTickEnabled(true);

            auto Transform = RunMesh->K2_GetComponentToWorld();

            RunMesh->K2_SetWorldLocation(Transform.Translation, false, nullptr, true);

            //RunMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        }
    }
    */

    uintptr_t GetNetMode = (uintptr_t(GetModuleHandle(0)) + 0xCFD600);
    uintptr_t GetNetMode2 = (uintptr_t(GetModuleHandle(0)) + 0x4B0180);

    MH_STATUS netmodeHookResult = MH_CreateHook((PVOID&)GetNetMode, GetNetModeHook, reinterpret_cast <LPVOID*> (&GetNetModeO));
    const char* netmodeHookResultString = MH_StatusToString(netmodeHookResult);

    if (netmodeHookResult != MH_STATUS::MH_OK)
    {
        printf("\n Hook GetNetMode CREATED FAILED WITH REASON : %s !\n", netmodeHookResultString);
    }
    else
    {
        if (MH_EnableHook((PVOID&)GetNetMode) != MH_STATUS::MH_OK)
        {
            printf("\n GetNetMode Hook ENABLE FAILED! \n");
        }
        else
        {
            std::cout << "GetNetMode Hook ENABLED" << std::endl;
        }
    }

    MH_STATUS netmodeHookResult2 = MH_CreateHook((PVOID&)GetNetMode2, GetNetModeHook, reinterpret_cast <LPVOID*> (&GetNetModeO));
    const char* netmodeHookResultString2 = MH_StatusToString(netmodeHookResult2);

    if (netmodeHookResult != MH_STATUS::MH_OK)
    {
        printf("\n Hook GetNetMode CREATED FAILED WITH REASON : %s !\n", netmodeHookResultString2);
    }
    else
    {
        if (MH_EnableHook((PVOID&)GetNetMode2) != MH_STATUS::MH_OK)
        {
            printf("\n GetNetMode Hook ENABLE FAILED! \n");
        }
        else
        {
            std::cout << "GetNetMode Hook ENABLED" << std::endl;
        }
    }

    uintptr_t ProcessEvent = (uintptr_t(GetModuleHandle(0)) + Offsets::ProcessEvent);

    MH_STATUS PROCESSEVENT = MH_CreateHook((PVOID&)ProcessEvent, ProcessEventHook, reinterpret_cast <LPVOID*> (&ProcessEventO));
    const char* ProcessEventResultString2 = MH_StatusToString(PROCESSEVENT);

    if (PROCESSEVENT != MH_STATUS::MH_OK)
    {
        printf("\n Hook ProcessEvent CREATED FAILED WITH REASON : %s !\n", ProcessEventResultString2);
    }
    else
    {
        if (MH_EnableHook((PVOID&)ProcessEvent) != MH_STATUS::MH_OK)
        {
            printf("\n ProcessEvent Hook ENABLE FAILED! \n");
        }
        else
        {
            std::cout << "ProcessEvent Hook ENABLED" << std::endl;
        }
    }


    Sleep(1000);
    //();

    switch (MapId)
    {
    case 1:
        std::cerr << "Loaded Erangel." << std::endl;
        SetupNormalGameMode();
        //DisableLevelStreaming();
        break;
    case 2:
        std::cerr << "Loaded Miramar." << std::endl;
        SetupNormalGameMode();
        //DisableLevelStreaming();
        break;
    case 3:
        std::cerr << "Loaded Sanhok." << std::endl;
        //SetupNormalGameMode();
        //ServerPawn->K2_TeleportTo(FVector(178.43, 268022, 178.43), ServerPawn->K2_GetActorRotation());
        //ServerPawn->FlushNetDormancy();
        //ServerPC->bMoveableAirborne = false;
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
        //ActorLoop();
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

void PrintAllPlayersCoordinates()
{
    SDK::UWorld* World = SDK::UWorld::GetWorld();
    if (!World) return;

    SDK::AGameStateBase* GS = UGameplayStatics::GetGameState(World);
    if (!GS) return;

    UC::TArray<SDK::APlayerState*>& Players = GS->PlayerArray;

    auto GMB = UGameplayStatics::GetGameMode(UWorld::GetWorld());
    auto GM = (AGameMode*)GMB;

    for (int i = 0; i < Players.Num(); i++)
    {
        SDK::APlayerState* PS = Players[i];
        if (!PS) continue;

        if (IsHostPlayer(PS, World)) {
            continue;
        }

        SDK::ASTExtraPlayerController* STEPC = (SDK::ASTExtraPlayerController*)PS->GetOwner();

        if (!STEPC)
            continue;

        if (PS->PlayerName.ToString().contains("Robot"))
            continue;

        SDK::ASTExtraBaseCharacter* Character = STEPC->STExtraBaseCharacter;

        std::cerr << Character->K2_GetActorLocation().Z << ", " << Character->K2_GetActorLocation().Y << ", " <<
            Character->K2_GetActorLocation().Z << std::endl;
    }
}

void FixValue()
{
    UWorld* World = UWorld::GetWorld();
    std::cerr << "World: " << World->GetName() << std::endl;

    float* value1 = (float*)(World->Pad_200 + 0x68);
    float* value2 = (float*)(World->Pad_260 + 0xB4);
    //float* value3 = (float*)(World->Pad_200);

    *value1 = 100.0f;
    *value2 = 100.0f;
    //*value3 = 100.0f;

    std::cerr << "Fixed item drop value" << std::endl;
}

void FixAntiDebugging()
{
    uint64* syscall = (uint64*)InSDKUtils::GetImageBase() + 0x4AF2074;

    printf("\n SYSCALL Value = %f \n", *syscall);
}

void ServerTeleportAllPlayersToLoc()
{
    for (int i = 0; i < UObject::GObjects->Num(); ++i)
    {
        UObject* Obj = UObject::GObjects->GetByIndex(i);

        if (!Obj || Obj->IsDefaultObject())
            continue;

        if (Obj->IsA(APawn::StaticClass()))
        {
            APawn* CurrentPawn = (APawn*)Obj;

            std::cerr << "ServerTeleportAllPlayersToLoc: ITERATING through player pawn: " << Obj->GetName() << std::endl;

            FTransform NewTransform;

            NewTransform.Translation = FVector(796360.19, 19990.86, 528.53); // Coordinates of the Spawn Island. 
            FQuat Rotation;

            Rotation.X = 0.0;
            Rotation.Y = 0.0;
            Rotation.Z = 0.0;
            Rotation.W = 1.0;

            NewTransform.Rotation = Rotation;
            NewTransform.Scale3D = FVector(1.0, 1.0, 1.0);

            struct FHitResult HitResultTeleport;

            CurrentPawn->K2_SetActorTransform(NewTransform, false, &HitResultTeleport, true);
        }
    }
}

void MatrixEffect()
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
    return 0;
}

void Log(const char* msg)
{
    OutputDebugStringA(msg);
}

void LogValue(const char* name, uintptr_t value)
{
    char buffer[256];
    sprintf_s(buffer, "%s: 0x%p\n", name, (void*)value);
    OutputDebugStringA(buffer);
}

void CheckItemSpots()
{
    std::cerr << "CheckItemStops" << std::endl;

    for (int i = 0; i < UObject::GObjects->Num(); ++i)
    {
        UObject* Obj = UObject::GObjects->GetByIndex(i);

        if (!Obj || Obj->IsDefaultObject())
            continue;

        if (Obj->IsA(ASTExtraHouseActor::StaticClass()))
        {
            ASTExtraHouseActor* house = (ASTExtraHouseActor*)Obj;
            std::cerr << "House: " << house->GetName() << std::endl;

            for (FVector pos : house->itemSpotPosList)
            {
                std::cerr << "Item pos: " << pos.X << ", " << pos.Y << ", " << pos.Z << std::endl;
            }
        }
    }
}

void CheckMovement(ASTExtraPlayerController* PC, APawn* Pawn)
{
    if (!PC || !Pawn)
        return;

    double Now = GetServerTime(); 

    FSpeedSample* Sample = nullptr;


    // Find existing player
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (SpeedSamples[i].Used &&
            SpeedSamples[i].Player == PC)
        {
            Sample = &SpeedSamples[i];
            break;
        }
    }


    // Create new entry
    if (!Sample)
    {
        for (int i = 0; i < MAX_PLAYERS; i++)
        {
            if (!SpeedSamples[i].Used)
            {
                SpeedSamples[i].Used = true;
                SpeedSamples[i].Player = PC;
                SpeedSamples[i].Suspicion = 0;
                SpeedSamples[i].LastServerTime = 0;

                Sample = &SpeedSamples[i];
                break;
            }
        }
    }


    if (!Sample)
    {
        return; // no slots available
    }

    FVector Current = Pawn->K2_GetActorLocation();

    if (Sample->LastServerTime == 0)
    {
        Sample->LastLocation = Current;
        Sample->LastServerTime = Now;
        return;
    }


    float DeltaTime = Now - Sample->LastServerTime;

    if (DeltaTime <= 0)
        return;

    float Distance = CalculateDistance(Current, Sample->LastLocation);


    float Speed = Distance / DeltaTime;


    constexpr float MaxAllowedSpeed = 800.f; // cm/s


    if (Speed > MaxAllowedSpeed)
    {
        Sample->Suspicion += 1.f;

        std::cerr << "Speed anomaly detected for player: " << PC->GetCurPlayerState()->PlayerName.ToString() << std::endl;
    }
    else
    {
        // slowly decay suspicion
        Sample->Suspicion -= 0.1f;

        if (Sample->Suspicion < 0.f)
        {
            Sample->Suspicion = 0.f;
        }
    }

    if (Sample->Suspicion > 5.f)
    {
        PC->STExtraBaseCharacter->Health = 0.0f;
        //PC->KickSelf();
        //PC->ClientLeaveMatchIntentionally();
        PC->ServerLeaveMatchIntentionally();

        ATslLPCPlayerState* ServerPS = (ATslLPCPlayerState*)UGameplayStatics::GetPlayerController(UWorld::GetWorld(), 0)->PlayerState;

        if (ServerPS->IsA(ATslLPCPlayerState::StaticClass()) && PC->GetCurPlayerState()->IsA(ATslLPCPlayerState::StaticClass()))
        {
            ServerPS->BroadcastMidGameBan((ATslLPCPlayerState*)PC->GetCurPlayerState(), FString(L"Banned"), FString(L"Banned"));
        }
        else
        {
            std::cerr << "Condition failed" << std::endl; 
        }
    }

    Sample->LastLocation = Current;
    Sample->LastServerTime = Now;
}

void hooksagain()
{
    uintptr_t GetNetMode = (uintptr_t(GetModuleHandle(0)) + 0xCFD600);
    uintptr_t GetNetMode2 = (uintptr_t(GetModuleHandle(0)) + 0x4B0180);

    MH_STATUS netmodeHookResult = MH_CreateHook((PVOID&)GetNetMode, GetNetModeHook, reinterpret_cast <LPVOID*> (&GetNetModeO));
    const char* netmodeHookResultString = MH_StatusToString(netmodeHookResult);

    if (netmodeHookResult != MH_STATUS::MH_OK)
    {
        printf("\n Hook GetNetMode CREATED FAILED WITH REASON : %s !\n", netmodeHookResultString);
    }
    else
    {
        if (MH_EnableHook((PVOID&)GetNetMode) != MH_STATUS::MH_OK)
        {
            printf("\n GetNetMode Hook ENABLE FAILED! \n");
        }
        else
        {
            std::cout << "GetNetMode Hook ENABLED" << std::endl;
        }
    }

    MH_STATUS netmodeHookResult2 = MH_CreateHook((PVOID&)GetNetMode2, GetNetModeHook, reinterpret_cast <LPVOID*> (&GetNetModeO));
    const char* netmodeHookResultString2 = MH_StatusToString(netmodeHookResult2);

    if (netmodeHookResult != MH_STATUS::MH_OK)
    {
        printf("\n Hook GetNetMode CREATED FAILED WITH REASON : %s !\n", netmodeHookResultString2);
    }
    else
    {
        if (MH_EnableHook((PVOID&)GetNetMode2) != MH_STATUS::MH_OK)
        {
            printf("\n GetNetMode Hook ENABLE FAILED! \n");
        }
        else
        {
            std::cout << "GetNetMode Hook ENABLED" << std::endl;
        }
    }

    uintptr_t ProcessEvent = (uintptr_t(GetModuleHandle(0)) + Offsets::ProcessEvent);

    MH_STATUS PROCESSEVENT = MH_CreateHook((PVOID&)ProcessEvent, ProcessEventHook, reinterpret_cast <LPVOID*> (&ProcessEventO));
    const char* ProcessEventResultString2 = MH_StatusToString(PROCESSEVENT);

    if (PROCESSEVENT != MH_STATUS::MH_OK)
    {
        printf("\n Hook ProcessEvent CREATED FAILED WITH REASON : %s !\n", ProcessEventResultString2);
    }
    else
    {
        if (MH_EnableHook((PVOID&)ProcessEvent) != MH_STATUS::MH_OK)
        {
            printf("\n ProcessEvent Hook ENABLE FAILED! \n");
        }
        else
        {
            std::cout << "ProcessEvent Hook ENABLED" << std::endl;
        }
    }
}

DWORD MainThread(HMODULE Module)
{
    /* Code to open a console window */
    AllocConsole();
    FILE* Dummy;
    freopen_s(&Dummy, "CONOUT$", "w", stderr);
    freopen_s(&Dummy, "CONOUT$", "w", stdout);
    freopen_s(&Dummy, "CONIN$", "r", stdin);

    std::cerr << "Waiting for GObjects..." << std::endl;

    do
    {
        Sleep(50);
    } while (!Dec::objobjects(Offsets::GObjects));

    Sleep(8600);

    MH_STATUS InitStatus = MH_Initialize();
    const char* StatusString = MH_StatusToString(InitStatus);
    printf("\n MINHOOK STATUS INIT: %s \n", StatusString);

    if (InitStatus != MH_OK)
    {
        printf("\n [!ERROR]: MINHOOK STATUS INIT : NOT OK !!!! -> ABORTING !\n");
    }

    /*
    void* Target = (void*)((uintptr_t)GetModuleHandle(nullptr) + 0x4D3140);
    void* Target2 = (void*)((uintptr_t)GetModuleHandle(nullptr) + 0x4CDD80);
    void* Target3 = (void*)((uintptr_t)GetModuleHandle(nullptr) + 0x4CCA30);

    if (MH_CreateHook(Target, &hkSub4D3140, reinterpret_cast<void**>(&oSub4D3140)) != MH_OK)
    {
        printf("4D3140 CreateHook failed\n");
    }

    if (MH_EnableHook(Target) != MH_OK)
    {
        printf("4D3140 EnableHook failed\n");
    }

    if (MH_CreateHook(Target2, &hkSub4CDD80, reinterpret_cast<void**>(&oSub4CDD80)) != MH_OK)
    {
        printf("4CDD80 CreateHook failed\n");
    }

    if (MH_EnableHook(Target2) != MH_OK)
    {
        printf("4CDD80 EnableHook failed\n");
    }

    if (MH_CreateHook(Target3, &hkGather, reinterpret_cast<void**>(&oGather)) != MH_OK)
    {
        printf("Gather CreateHook failed\n");
    }

    if (MH_EnableHook(Target3) != MH_OK)
    {
        printf("Gather EnableHook failed\n");
    }
    */

    //InitUEConsole();

    UWorld* World = UWorld::GetWorld();

    UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"t.MaxFPS 60"), UGameplayStatics::GetPlayerController(UWorld::GetWorld(), 0));

    UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"s.LoadAllStreamingLevels 1"), UGameplayStatics::GetPlayerController(UWorld::GetWorld(), 0));
    UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"s.UseBackgroundLevelStreaming 0"), UGameplayStatics::GetPlayerController(UWorld::GetWorld(), 0));
    UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"s.AsyncLoadingThreadEnabled 0"), UGameplayStatics::GetPlayerController(UWorld::GetWorld(), 0));
    UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"LoadAllLand 1"), UGameplayStatics::GetPlayerController(UWorld::GetWorld(), 0));

    std::cerr << "Ready" << std::endl;

    //MatrixEffect();

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
                                                                                      )" << std::endl;

    printf("\x54""H\111S\x20""I\123 \x46""R\105E\x20""C\117N\x54""E\116T\x20""B\131 \x4F""G\072B\x41""T\124L\x45""G\122O\x55""N\104S\x2C"" \111F\x20""Y\117U\x20""B\117U\x47""H\124 \x54""H\111S\x2C"" \131O\x55"" \110A\x56""E\040B\x45""E\116 \x53""C\101M\x4D""E\104.\x20""h\164t\x70""s\072/\x2F""g\151t\x68""u\142.\x63""o\155/\x48""4\124I\x55""X\012");

    int Map;

    std::cerr << "Select map" << std::endl;
    std::cerr << "1. Erangel" << std::endl;
    std::cerr << "2. Miramar" << std::endl;
    std::cerr << "3. Sanhok" << std::endl;
    std::cerr << "4. Bodie (TDM)" << std::endl;
    std::cerr << "5. Periverka (TDM)" << std::endl;
    std::cerr << "6. Training Mode" << std::endl;
    std::cerr << "7. Vikendi" << std::endl;
    std::cerr << "8. Continue" << std::endl;

    std::cin >> Map;

    switch (Map) {
    case 1:
        UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), FString(L"open PUBG_Forest?listen"), nullptr);
        OnStartMap(1);
        break;
    case 2:
        UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), FString(L"open PUBG_Desert?listen"), nullptr);
        OnStartMap(2);
        break;
    case 3:
        UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), FString(L"open /Game/Maps/PUBG_Savage/PUBG_Savage_Main?listen"), nullptr);
        OnStartMap(3);
        break;
    case 4:
        UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), FString(L"open PUBG_Bodie_Main?game=/Game/Blueprints/Core/TeamMatchMode/BP_BattleRoyaleTeamMatchGameMode_C?listen"), nullptr);
        OnStartMap(4);
        break;
    case 5:
        UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), FString(L"open PUBG_School_Main?listen"), nullptr);
        OnStartMap(5);
        break;
    case 6:
        UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), FString(L"open shooting_range4?listen"), nullptr);
        bIsTrainingMode = true;
        while (!UWorld::GetWorld()) Sleep(50);
        Sleep(4000);
        hooksagain();
        //OnStartMap(6);
        break;
    case 7:
        UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), FString(L"open /Game/Maps/PUBG_DihorOtok/DihorOtok_Main?listen"), nullptr);
        OnStartMap(1);
        break;
    case 8:
        break;

    default:
        std::cerr << "Invalid map selection" << std::endl;
        break;
    }
    

    while (true)
    {
        SafeProcessPlayers(); 
        Sleep(100);
    }

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        CreateThread(0, 0, (LPTHREAD_START_ROUTINE)MainThread, hModule, 0, 0);
        break;
    }

    return TRUE;
}