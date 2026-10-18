#define NOMINMAX

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

static HMODULE g_hModule = nullptr;

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
volatile bool gRequestBuildingRefresh = 0;
volatile bool gRequestErangelLoot = 0;

volatile bool serverPanelInitialized = 0;
volatile bool bIsLootFinished = 0;

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

static bool s_VisualWrappersInitialized = false;
static SDK::UClass* s_BaseWrapper = nullptr;

// Weapons
static SDK::UClass* s_AUG = nullptr;        // AR (SM_M416 mesh)
static SDK::UClass* s_Groza = nullptr;      // AR / SMG (SM_Groza mesh)
static SDK::UClass* s_M249 = nullptr;       // LMG (SM_M249 mesh)
static SDK::UClass* s_AWM = nullptr;        // Sniper (SM_AWM mesh)
static SDK::UClass* s_M24 = nullptr;        // Sniper (SM_M24 mesh)
static SDK::UClass* s_Mk14 = nullptr;       // DMR (SM_Mk14 mesh)
static SDK::UClass* s_Kar98 = nullptr;
static SDK::UClass* s_SKS = nullptr;
static SDK::UClass* s_Mini14 = nullptr;
static SDK::UClass* s_SLR = nullptr;
static SDK::UClass* s_M416 = nullptr;
static SDK::UClass* s_AKM = nullptr;
static SDK::UClass* s_SCAR = nullptr;
static SDK::UClass* s_Beryl = nullptr;
static SDK::UClass* s_UMP = nullptr;
static SDK::UClass* s_Uzi = nullptr;
static SDK::UClass* s_Vector = nullptr;
static SDK::UClass* s_Tommy = nullptr;
static SDK::UClass* s_S686 = nullptr;
static SDK::UClass* s_S1897 = nullptr;
static SDK::UClass* s_S12K = nullptr;
static SDK::UClass* s_P92 = nullptr;
static SDK::UClass* s_P1911 = nullptr;
static SDK::UClass* s_P18C = nullptr;
static SDK::UClass* s_Crossbow = nullptr;
static SDK::UClass* s_Pan = nullptr;
static SDK::UClass* s_Crowbar = nullptr;
static SDK::UClass* s_Machete = nullptr;

// Ammo
static SDK::UClass* s_Ammo556 = nullptr;    // 5.56mm ammo box mesh
static SDK::UClass* s_Ammo762 = nullptr;    // 7.62mm ammo box mesh
static SDK::UClass* s_Ammo300 = nullptr;    // .300 Magnum ammo box mesh
static SDK::UClass* s_Ammo9mm = nullptr;
static SDK::UClass* s_Ammo45 = nullptr;
static SDK::UClass* s_Ammo12G = nullptr;

// Armor & Gear
static SDK::UClass* s_Armor1 = nullptr;
static SDK::UClass* s_Armor2 = nullptr;
static SDK::UClass* s_Armor3 = nullptr;     // Level 3 Armor mesh
static SDK::UClass* s_Helmet1 = nullptr;
static SDK::UClass* s_Helmet2 = nullptr;
static SDK::UClass* s_Helmet3 = nullptr;    // Level 3 Helmet mesh
static SDK::UClass* s_Bag1 = nullptr;
static SDK::UClass* s_Bag2 = nullptr;
static SDK::UClass* s_Bag3 = nullptr;       // Level 3 Backpack mesh
static SDK::UClass* s_Ghillie = nullptr;    // Ghillie Suit mesh

// Medical / Consumables
static SDK::UClass* s_FirstAid = nullptr;   // First Aid kit mesh
static SDK::UClass* s_Injection = nullptr;  // Adrenaline / painkiller mesh
static SDK::UClass* s_GasCan = nullptr;

// Weapon Attachments & Scopes
static SDK::UClass* s_Scope4x = nullptr;
static SDK::UClass* s_Scope8x = nullptr;
static SDK::UClass* s_CompAR = nullptr;
static SDK::UClass* s_CompSR = nullptr;
static SDK::UClass* s_Stock = nullptr;
static SDK::UClass* s_RedDot = nullptr;
static SDK::UClass* s_GripVert = nullptr;
static SDK::UClass* s_MagAR = nullptr;
static SDK::UClass* s_MagSR = nullptr;

// Throwables
static SDK::UClass* s_GrenadeFrag = nullptr;
static SDK::UClass* s_GrenadeSmoke = nullptr;
static SDK::UClass* s_GrenadeMolotov = nullptr;
static SDK::UClass* s_GrenadeStun = nullptr;

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

using ShouldFn = bool(__fastcall*)(ULevelStreaming*);

ShouldFn oShouldBeLoaded;
ShouldFn oShouldBeVisible;

/*
VTable[70] = 00007FF79E919FE0 // 1389FE0
VTable[71] = 00007FF79E919FF0 // 1389FF0
*/

bool IsGameplayStreamingLevel(ULevelStreaming* level)
{
    //if (!level->IsA(ULevelStreamingKismet::StaticClass()) || level->GetName().contains("32")) return false;

    return true;
}

bool __fastcall hkShouldBeLoaded(ULevelStreaming* level)
{
    //if (IsGameplayStreamingLevel(level))
    return true;

    //return oShouldBeLoaded(level);
}

bool __fastcall hkShouldBeVisible(ULevelStreaming* level)
{
    //if (IsGameplayStreamingLevel(level))
    return true;

    //return oShouldBeVisible(level);
}

volatile bool gStreamingHooksInstalled = false;

void InstallStreamingHooks(ULevelStreaming* anyStreamingObject)
{
    /*
    void** vtable = *(void***)anyStreamingObject;

    DWORD old;
    VirtualProtect(&vtable[70], sizeof(void*) * 2, PAGE_EXECUTE_READWRITE, &old);

    oShouldBeLoaded = (ShouldFn)vtable[70];
    oShouldBeVisible = (ShouldFn)vtable[71];

    vtable[70] = (void*)&hkShouldBeLoaded;
    vtable[71] = (void*)&hkShouldBeVisible;

    VirtualProtect(&vtable[70], sizeof(void*) * 2, old, &old);
    */



    
    uintptr_t func_Addr = (uintptr_t(GetModuleHandle(0)) + 0x1389FE0); // ShouldBeLoaded

    oShouldBeLoaded = decltype(oShouldBeLoaded)(func_Addr);
    MH_STATUS HookResult = MH_CreateHook((PVOID&)func_Addr, hkShouldBeLoaded, reinterpret_cast <LPVOID*> (&oShouldBeLoaded));
    const char* HookResultString = MH_StatusToString(HookResult);

    if (HookResult != MH_STATUS::MH_OK)
    {
        printf("\n Hook hkShouldBeLoaded CREATED FAILED WITH REASON : %s !\n", HookResultString);
    }
    else
    {
        if (MH_EnableHook((PVOID&)func_Addr) != MH_STATUS::MH_OK)
        {
            printf("\n hkShouldBeLoaded Hook ENABLE FAILED! \n");
        }
        else
        {
            printf("\n hkShouldBeLoaded Hook Enabled! \n");
        }
    }


    uintptr_t func_Addr2 = (uintptr_t(GetModuleHandle(0)) + 0x1389FF0); // ShouldBeVisible

    oShouldBeVisible = decltype(oShouldBeVisible)(func_Addr2);
    MH_STATUS HookResult2 = MH_CreateHook((PVOID&)func_Addr, hkShouldBeLoaded, reinterpret_cast <LPVOID*> (&oShouldBeVisible));
    const char* HookResultString2 = MH_StatusToString(HookResult2);

    if (HookResult2 != MH_STATUS::MH_OK)
    {
        printf("\n Hook hkShouldBeVisible CREATED FAILED WITH REASON : %s !\n", HookResultString2);
    }
    else
    {
        if (MH_EnableHook((PVOID&)func_Addr2) != MH_STATUS::MH_OK)
        {
            printf("\n hkShouldBeVisible Hook ENABLE FAILED! \n");
        }
        else
        {
            printf("\n hkShouldBeVisible Hook Enabled! \n");
        }
    }
}

int DisableCullingForStreamingLevels(SDK::UWorld* World)
{
    if (!World)
    {
        printf("DisableCullingForAllActors: World is null.\n");
        return 0;
    }

    printf("\nDisableCullingForAllActors: World is valid, proceeding.\n");

    auto pc = SDK::UGameplayStatics::GetPlayerController(World, 0);
    auto stpc = (SDK::ASTExtraPlayerController*)pc;

    // Ignore persistent level, focus on streaming levels
    if (World->WorldComposition)
    {
        for (SDK::ULevelStreaming* StreamingLevel : World->WorldComposition->TilesStreaming)
        {
            if (!gStreamingHooksInstalled)
            {
                if (StreamingLevel)
                {
                    std::string LevelName = StreamingLevel->GetName();

                    printf("We are iterating the level: %s\n", LevelName.c_str());

                    InstallStreamingHooks(StreamingLevel);
                    gStreamingHooksInstalled = true;
                    break;
                }
            }
        }
    }

    /*
    if (stpc)
    {
        stpc->LoadAllLand(true);
        stpc->bIsBattleOwner = true;
        stpc->bIsGM = true;
    }
    */

    return 0;
}

void InitVisualWrapperClasses()
{
    std::cerr << "InitVisualWrapperClasses" << std::endl;

    if (s_VisualWrappersInitialized) { std::cerr << "Already initialized." << std::endl; return; }

    s_BaseWrapper = SDK::APickUpWrapperActor::StaticClass();

    int foundCount = 0;
    int totalObjects = SDK::UObject::GObjects->Num();

    for (int i = 0; i < totalObjects; ++i)
    {
        SDK::UObject* Obj = SDK::UObject::GObjects->GetByIndex(i);
        if (!Obj) continue;

        if (!Obj->HasTypeFlag(SDK::EClassCastFlags::Class)) continue;

        std::string name = Obj->GetName();

        // Core preloaded visual wrappers
        if (!s_AUG && name == "BP_Rifle_AUG_Wrapper_C") { s_AUG = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_Groza && name == "BP_Rifle_Groza_Wrapper_C") { s_Groza = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_M249 && name == "BP_Other_M249_Wrapper_C") { s_M249 = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_AWM && name == "BP_Sniper_AWM_Wrapper_C") { s_AWM = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_M24 && name == "BP_Sniper_M24_Wrapper_C") { s_M24 = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_Mk14 && name == "BP_WEP_Mk14_Pickup_C") { s_Mk14 = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_Ammo556 && name == "BP_Ammo_556mm_Pickup_C") { s_Ammo556 = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_Ammo762 && name == "BP_Ammo_762mm_Pickup_C") { s_Ammo762 = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_Ammo300 && name == "BP_Ammo_300Magnum_Pickup_C") { s_Ammo300 = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_Armor3 && name == "PickUp_BP_Armor_Lv3_C") { s_Armor3 = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_Helmet3 && name == "PickUp_BP_Helmet_Lv3_C") { s_Helmet3 = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_Bag3 && name == "PickUp_BP_Bag_Lv3_C") { s_Bag3 = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_Ghillie && name == "PickUp_BP_Ghillie_1_C") { s_Ghillie = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_FirstAid && name == "FirstAidbox_Pickup_C") { s_FirstAid = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_Injection && name == "Injection_Pickup_C") { s_Injection = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_Scope4x && name == "BP_MZJ_4X_Pickup_C") { s_Scope4x = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_Scope8x && name == "BP_MZJ_8X_Pickup_C") { s_Scope8x = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_CompAR && name == "BP_QK_Large_Compensator_Pickup_C") { s_CompAR = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_CompSR && name == "BP_QK_Sniper_Compensator_Pickup_C") { s_CompSR = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_Stock && name == "BP_QT_A_Pickup_C") { s_Stock = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_RedDot && name == "BP_SideRail_Dotsight_Pickup_C") { s_RedDot = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_GripVert && name == "BP_WB_Vertical_Pickup_C") { s_GripVert = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_MagAR && name == "BP_DJ_Large_EQ_Pickup_C") { s_MagAR = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_MagSR && name == "BP_DJ_Sniper_EQ_Pickup_C") { s_MagSR = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_GrenadeFrag && name == "BP_Grenade_Shoulei_Weapon_Wrapper_C") { s_GrenadeFrag = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_GrenadeSmoke && name == "BP_Grenade_Smoke_Weapon_Wrapper_C") { s_GrenadeSmoke = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_GrenadeMolotov && name == "BP_Grenade_Burn_Weapon_Wrapper_C") { s_GrenadeMolotov = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_GrenadeStun && name == "BP_Grenade_Stun_Weapon_Wrapper_C") { s_GrenadeStun = (SDK::UClass*)Obj; foundCount++; }

        // Dynamic extra weapon wrappers if loaded
        else if (!s_M416 && (name == "BP_Rifle_M416_Wrapper_C" || name == "BP_WEP_M416_Pickup_C")) { s_M416 = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_AKM && (name == "BP_Rifle_AKM_Wrapper_C" || name == "BP_WEP_AKM_Pickup_C")) { s_AKM = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_SCAR && (name == "BP_Rifle_SCAR_Wrapper_C" || name == "BP_WEP_SCAR_Pickup_C")) { s_SCAR = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_Beryl && (name == "BP_Rifle_BerylM762_Wrapper_C" || name == "BP_WEP_BerylM762_Pickup_C")) { s_Beryl = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_Kar98 && (name == "BP_Sniper_Kar98k_Wrapper_C" || name == "BP_WEP_Kar98k_Pickup_C")) { s_Kar98 = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_SKS && (name == "BP_Sniper_SKS_Wrapper_C" || name == "BP_WEP_SKS_Pickup_C")) { s_SKS = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_Mini14 && (name == "BP_Sniper_Mini14_Wrapper_C" || name == "BP_WEP_Mini14_Pickup_C")) { s_Mini14 = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_SLR && (name == "BP_Sniper_SLR_Wrapper_C" || name == "BP_WEP_SLR_Pickup_C")) { s_SLR = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_UMP && (name == "BP_Other_UMP45_Wrapper_C" || name == "BP_WEP_UMP9_Pickup_C" || name == "BP_WEP_UMP45_Pickup_C")) { s_UMP = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_Uzi && (name == "BP_Other_UZI_Wrapper_C" || name == "BP_WEP_UZI_Pickup_C")) { s_Uzi = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_Vector && (name == "BP_Other_Vector_Wrapper_C" || name == "BP_WEP_Vector_Pickup_C")) { s_Vector = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_Tommy && (name == "BP_Other_TommyGun_Wrapper_C" || name == "BP_WEP_TommyGun_Pickup_C")) { s_Tommy = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_S686 && (name == "BP_Shotgun_S686_Wrapper_C" || name == "BP_WEP_S686_Pickup_C")) { s_S686 = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_S1897 && (name == "BP_Shotgun_S1897_Wrapper_C" || name == "BP_WEP_S1897_Pickup_C")) { s_S1897 = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_S12K && (name == "BP_Shotgun_S12K_Wrapper_C" || name == "BP_WEP_S12K_Pickup_C")) { s_S12K = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_P92 && (name == "BP_Pistol_P92_Wrapper_C" || name == "BP_WEP_P92_Pickup_C")) { s_P92 = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_P1911 && (name == "BP_Pistol_P1911_Wrapper_C" || name == "BP_WEP_P1911_Pickup_C")) { s_P1911 = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_P18C && (name == "BP_Pistol_P18C_Wrapper_C" || name == "BP_WEP_P18C_Pickup_C")) { s_P18C = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_Crossbow && (name == "BP_Other_Crossbow_Wrapper_C" || name == "BP_WEP_Crossbow_Pickup_C")) { s_Crossbow = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_Pan && (name == "BP_Melee_Pan_Wrapper_C" || name == "BP_WEP_Pan_Pickup_C")) { s_Pan = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_Crowbar && (name == "BP_Melee_Crowbar_Wrapper_C" || name == "BP_WEP_Crowbar_Pickup_C")) { s_Crowbar = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_Machete && (name == "BP_Melee_Machete_Wrapper_C" || name == "BP_WEP_Machete_Pickup_C")) { s_Machete = (SDK::UClass*)Obj; foundCount++; }

        // Dynamic extra gear
        else if (!s_Helmet1 && name == "PickUp_BP_Helmet_Lv1_C") { s_Helmet1 = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_Helmet2 && name == "PickUp_BP_Helmet_Lv2_C") { s_Helmet2 = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_Armor1 && name == "PickUp_BP_Armor_Lv1_C") { s_Armor1 = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_Armor2 && name == "PickUp_BP_Armor_Lv2_C") { s_Armor2 = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_Bag1 && name == "PickUp_BP_Bag_Lv1_C") { s_Bag1 = (SDK::UClass*)Obj; foundCount++; }
        else if (!s_Bag2 && name == "PickUp_BP_Bag_Lv2_C") { s_Bag2 = (SDK::UClass*)Obj; foundCount++; }
    }

    printf("[VisualLoot] Resolved %d visual mesh wrapper classes in 1 pass across GObjects (AUG=%p, Groza=%p, M24=%p, AWM=%p, Ammo556=%p, Ammo762=%p, Helmet=%p, Armor=%p, Bag=%p)\n",
        foundCount, s_AUG, s_Groza, s_M24, s_AWM, s_Ammo556, s_Ammo762, s_Helmet3, s_Armor3, s_Bag3);

    s_VisualWrappersInitialized = true;
}

SDK::UClass* GetVisualWrapperClass(SDK::EBackpackItemType ItemType, int32_t SpecificID)
{
    if (!s_VisualWrappersInitialized)
    {
        InitVisualWrapperClasses();
    }

    SDK::UClass* Chosen = nullptr;

    switch (ItemType)
    {
    case SDK::EBackpackItemType::BackpackItemType_Weapon:
    {
        switch (SpecificID)
        {
        case 101004: // M416
            Chosen = s_M416 ? s_M416 : s_AUG;
            break;
        case 101003: // SCAR-L
            Chosen = s_SCAR ? s_SCAR : (s_AUG ? s_AUG : s_Groza);
            break;
        case 101001: // AKM
            Chosen = s_AKM ? s_AKM : (s_Groza ? s_Groza : s_AUG);
            break;
        case 101008: // Beryl M762
            Chosen = s_Beryl ? s_Beryl : (s_Groza ? s_Groza : s_AUG);
            break;
        case 101005: // Groza
            Chosen = s_Groza ? s_Groza : s_AUG;
            break;
        case 103001: // Kar98k
            Chosen = s_Kar98 ? s_Kar98 : (s_M24 ? s_M24 : s_AWM);
            break;
        case 103002: // M24
            Chosen = s_M24 ? s_M24 : s_AWM;
            break;
        case 103003: // AWM
            Chosen = s_AWM ? s_AWM : s_M24;
            break;
        case 103004: // SKS
            Chosen = s_SKS ? s_SKS : (s_Mk14 ? s_Mk14 : s_M24);
            break;
        case 103006: // Mini14
            Chosen = s_Mini14 ? s_Mini14 : (s_Mk14 ? s_Mk14 : s_M24);
            break;
        case 103009: // SLR
            Chosen = s_SLR ? s_SLR : (s_Mk14 ? s_Mk14 : s_M24);
            break;
        case 105001: // M249
            Chosen = s_M249 ? s_M249 : s_AUG;
            break;
        case 102001: // Micro Uzi
            Chosen = s_Uzi ? s_Uzi : (s_Groza ? s_Groza : s_AUG);
            break;
        case 102002: // UMP45
            Chosen = s_UMP ? s_UMP : (s_Groza ? s_Groza : s_AUG);
            break;
        case 102003: // Vector
            Chosen = s_Vector ? s_Vector : (s_Groza ? s_Groza : s_AUG);
            break;
        case 102004: // Tommy Gun
            Chosen = s_Tommy ? s_Tommy : (s_Groza ? s_Groza : s_AUG);
            break;
        case 104001: // S686
            Chosen = s_S686 ? s_S686 : (s_M24 ? s_M24 : s_AUG);
            break;
        case 104002: // S1897
            Chosen = s_S1897 ? s_S1897 : (s_M24 ? s_M24 : s_AUG);
            break;
        case 104003: // S12K
            Chosen = s_S12K ? s_S12K : (s_AUG ? s_AUG : s_Groza);
            break;
        case 106001: // P92 / Frag grenade slot fallback
            Chosen = s_P92 ? s_P92 : (s_RedDot ? s_RedDot : s_AUG);
            break;
        case 106002: // P1911 / Smoke grenade slot fallback
            Chosen = s_P1911 ? s_P1911 : (s_RedDot ? s_RedDot : s_AUG);
            break;
        case 106004: // P18C / Stun grenade slot fallback
            Chosen = s_P18C ? s_P18C : (s_RedDot ? s_RedDot : s_AUG);
            break;
        case 108001: // Crossbow
            Chosen = s_Crossbow ? s_Crossbow : (s_M24 ? s_M24 : s_AUG);
            break;
        case 108002: // Crowbar
            Chosen = s_Crowbar ? s_Crowbar : (s_Stock ? s_Stock : s_RedDot);
            break;
        case 108003: // Machete / Flare Gun
            Chosen = s_Machete ? s_Machete : (s_Stock ? s_Stock : s_RedDot);
            break;
        case 108004: // Pan
            Chosen = s_Pan ? s_Pan : (s_Stock ? s_Stock : s_RedDot);
            break;
            // Throwables
        case 401001: Chosen = s_GrenadeFrag; break;
        case 401002: Chosen = s_GrenadeSmoke; break;
        case 401003: Chosen = s_GrenadeStun; break;
        case 401004: Chosen = s_GrenadeMolotov; break;
        default:
            Chosen = s_AUG ? s_AUG : s_Groza;
            break;
        }
        break;
    }
    case SDK::EBackpackItemType::BackpackItemType_Ammo:
    {
        switch (SpecificID)
        {
        case 303001: // 5.56mm
            Chosen = s_Ammo556 ? s_Ammo556 : s_Ammo762;
            break;
        case 302001: // 7.62mm
            Chosen = s_Ammo762 ? s_Ammo762 : s_Ammo556;
            break;
        case 306001: // .300 Magnum
            Chosen = s_Ammo300 ? s_Ammo300 : s_Ammo762;
            break;
        case 301001: // 9mm
            Chosen = s_Ammo9mm ? s_Ammo9mm : (s_Ammo556 ? s_Ammo556 : s_Ammo762);
            break;
        case 305001: // .45 ACP
            Chosen = s_Ammo45 ? s_Ammo45 : (s_Ammo762 ? s_Ammo762 : s_Ammo556);
            break;
        case 304001: // 12 Gauge
            Chosen = s_Ammo12G ? s_Ammo12G : (s_Ammo762 ? s_Ammo762 : s_Ammo556);
            break;
        default:
            Chosen = s_Ammo556 ? s_Ammo556 : s_Ammo762;
            break;
        }
        break;
    }
    case SDK::EBackpackItemType::BackpackItemType_Armor:
    {
        if (SpecificID == 501001) Chosen = s_Bag1 ? s_Bag1 : s_Bag3;
        else if (SpecificID == 501002) Chosen = s_Bag2 ? s_Bag2 : s_Bag3;
        else if (SpecificID == 501003) Chosen = s_Bag3;
        else if (SpecificID == 502001) Chosen = s_Helmet1 ? s_Helmet1 : s_Helmet3;
        else if (SpecificID == 502002) Chosen = s_Helmet2 ? s_Helmet2 : s_Helmet3;
        else if (SpecificID == 502003) Chosen = s_Helmet3;
        else if (SpecificID == 503001) Chosen = s_Armor1 ? s_Armor1 : s_Armor3;
        else if (SpecificID == 503002) Chosen = s_Armor2 ? s_Armor2 : s_Armor3;
        else if (SpecificID == 503003) Chosen = s_Armor3;
        else if (SpecificID >= 501000 && SpecificID < 502000) Chosen = s_Bag3;
        else if (SpecificID >= 502000 && SpecificID < 503000) Chosen = s_Helmet3;
        else if (SpecificID >= 503000 && SpecificID < 504000) Chosen = s_Armor3;
        else Chosen = s_Armor3 ? s_Armor3 : s_Helmet3;
        break;
    }
    case SDK::EBackpackItemType::BackpackItemType_Clothing:
    {
        Chosen = s_Ghillie ? s_Ghillie : s_Armor3;
        break;
    }
    case SDK::EBackpackItemType::BackpackItemType_Consumable:
    {
        switch (SpecificID)
        {
        case 601002: // First Aid
        case 601001: // Bandages
        case 601003: // Med Kit
            Chosen = s_FirstAid ? s_FirstAid : s_Injection;
            break;
        case 601006: // Energy Drink
        case 601004: // Painkiller
        case 601005: // Adrenaline
            Chosen = s_Injection ? s_Injection : s_FirstAid;
            break;
        case 602001: // Gas Can
            Chosen = s_GasCan ? s_GasCan : (s_FirstAid ? s_FirstAid : s_Injection);
            break;
        default:
            Chosen = s_FirstAid ? s_FirstAid : s_Injection;
            break;
        }
        break;
    }
    case SDK::EBackpackItemType::BackpackItemType_WeaponAttachment:
    {
        switch (SpecificID)
        {
        case 201001: // Red Dot
        case 201002: // Holo
            Chosen = s_RedDot;
            break;
        case 201003: // 2x
        case 201009: // 3x
        case 201004: // 4x
            Chosen = s_Scope4x ? s_Scope4x : s_Scope8x;
            break;
        case 201005: // 6x
        case 201006: // 8x
            Chosen = s_Scope8x ? s_Scope8x : s_Scope4x;
            break;
        case 202001: // AR Comp
        case 202004: // AR Supp
        case 202006: // SMG Supp
            Chosen = s_CompAR ? s_CompAR : s_CompSR;
            break;
        case 202005: // SR Supp
            Chosen = s_CompSR ? s_CompSR : s_CompAR;
            break;
        case 204003: // AR Ext Quickdraw
            Chosen = s_MagAR ? s_MagAR : s_CompAR;
            break;
        case 204006: // SR Ext Quickdraw
            Chosen = s_MagSR ? s_MagSR : s_CompSR;
            break;
        case 203001: // Vert Grip
        case 203002: // Angled Grip
            Chosen = s_GripVert;
            break;
        default:
            Chosen = s_Stock ? s_Stock : s_CompAR;
            break;
        }
        break;
    }
    default:
        break;
    }

    if (Chosen) {
        //std::cerr << "RETURNING: " << Chosen->GetName() << std::endl;
    }

    if (!Chosen) {
        //std::cerr << "RETURNING: " << s_BaseWrapper->GetName() << std::endl; 
        Chosen = s_BaseWrapper;
    }
    if (!Chosen) {
        Chosen = SDK::APickUpWrapperActor::StaticClass();
        //std::cerr << "RETURNING: " << Chosen->GetName() << std::endl;
    }

    return Chosen;
}

bool SpawnLootItemAtLocation(
    SDK::UWorld* World,
    const SDK::FVector& Location,
    SDK::EBackpackItemType ItemType,
    int32_t SpecificID,
    int32_t Count,
    SDK::AActor* Owner = nullptr
)
{
    if (!World) return false;

    SDK::FTransform SpawnTransform{};
    SpawnTransform.Translation = Location;
    SpawnTransform.Translation.Z += 32.0f; // Sits cleanly above floorboards/tables to prevent clipping
    SpawnTransform.Rotation = SDK::FQuat{ 0.0f, 0.0f, 0.0f, 1.0f };
    SpawnTransform.Scale3D = SDK::FVector(1.0f, 1.0f, 1.0f);

    SDK::UClass* WrapperClass = GetVisualWrapperClass(ItemType, SpecificID);
    if (!WrapperClass) WrapperClass = SDK::APickUpWrapperActor::StaticClass();
    if (!WrapperClass) return false;

    SDK::AActor* GenericActor = nullptr;
    __try
    {
        GenericActor = SDK::UGameplayStatics::BeginDeferredActorSpawnFromClass(
            World,
            WrapperClass,
            SpawnTransform,
            SDK::ESpawnActorCollisionHandlingMethod::AlwaysSpawn,
            Owner
        );
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        std::cerr << "SEH spawning actor" << std::endl;
        return false;
    }

    
    if (!GenericActor) {
        std::cerr << "GenericActor is still nullptr" << std::endl;
        return false;
    }

    SDK::APickUpWrapperActor* Pickup = reinterpret_cast<SDK::APickUpWrapperActor*>(GenericActor);

    SDK::FItemDefineID ItemID{};
    ItemID.Type = (int32_t)ItemType;
    ItemID.TypeSpecificID = SpecificID;
    ItemID.AvatarID = 0;
    ItemID.bValidItem = true;
    ItemID.bValidInstance = true;
    ItemID.InstanceID = SDK::UBackpackUtils::GenerateRandomInstanceID();

    Pickup->DefineID = ItemID;
    Pickup->Count = Count;
    Pickup->bDropedByPlayer = true;
    //Pickup->bDropedByPlayer = false;

    __try
    {
        SDK::UGameplayStatics::FinishSpawningActor(Pickup, SpawnTransform);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        std::cerr << "SEH FinishSpawningActor" << std::endl;
        return false;
    }

    Pickup->bAlwaysRelevant = false;
    Pickup->NetPriority = 1.5f;
    Pickup->NetCullDistanceSquared = 15000.0f * 15000.0f; // 150m cull distance

    __try { Pickup->SetDefineID(ItemID); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    __try { Pickup->SetCountOnServerAfterSpawn(Count); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    __try { Pickup->MarkIsDropedByPlayerOnServer(1); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    __try { Pickup->ShowActor(); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    __try { Pickup->ShowMesh(1); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}

    return true;
}

// Authentic PUBG Loot Tables
struct MapWeaponDef
{
    int32_t WeaponID;
    int32_t AmmoID;
    int32_t AmmoCount;
    const char* Name;
};

static const MapWeaponDef kMapWeapons[] = {
    // Assault Rifles
    { 101004, 303001, 30, "M416" },
    { 101003, 303001, 30, "SCAR-L" },
    { 101007, 303001, 30, "QBZ"},
    { 101001, 303001, 30, "AKM" },
    { 101008, 302001, 30, "Beryl M762" },
    //{ 101005, 302001, 30, "Groza" },
    { 101009, 302001, 30, "Mk47 Mutant"},
    { 105002, 302001, 60, "DP-28"},


    // Sniper Rifles & DMRs
    { 103001, 302001, 20, "Kar98k" },
    { 103002, 302001, 20, "M24" },
    { 103004, 302001, 30, "SKS" },
    { 103006, 303001, 30, "Mini14" },
    { 103009, 302001, 30, "SLR" },
    //{ 103003, 304001, 15, "AWM" },

    // SMGs
    { 102002, 305001, 30, "UMP45" },
    { 102001, 301001, 30, "Micro Uzi" },
    { 102003, 301001, 30, "Vector" },
    { 102004, 305001, 30, "Tommy Gun" },

    // Shotguns
    { 104001, 304001, 15, "S686" },
    { 104002, 304001, 15, "S1897" },
    { 104003, 304001, 15, "S12K" },

    // Pistols
    { 106001, 301001, 30, "P92" },
    { 106002, 305001, 25, "P1911" },
    { 106004, 301001, 40, "P18C" },

    // Special / Melee
    { 108001, 307001, 15, "Crossbow" },
    { 108004,      0,  0, "Pan" },
    { 108002,      0,  0, "Crowbar" },
    { 108003,      0,  0, "Machete" },
    { 108003, 308001,  1, "Flare Gun" }
};

struct MapAmmoDef
{
    int32_t AmmoID;
    int32_t Count;
    const char* Name;
};

static const MapAmmoDef kMapAmmo[] = {
    { 301001, 30, "9mm Ammo" },
    { 303001, 30, "5.56mm Ammo" },
    { 302001, 30, "7.62mm Ammo" },
    { 305001, 30, ".45 ACP Ammo" },
    { 304001, 5, "12 Gauge Ammo" }
};

struct MapGearDef
{
    SDK::EBackpackItemType Type;
    int32_t SpecificID;
    const char* Name;
};

static const MapGearDef kMapGear[] = {
    { SDK::EBackpackItemType::BackpackItemType_Armor, 501001, "Level 1 Backpack" },
    { SDK::EBackpackItemType::BackpackItemType_Armor, 501002, "Level 2 Backpack" },
    { SDK::EBackpackItemType::BackpackItemType_Armor, 501003, "Level 3 Backpack" },
    { SDK::EBackpackItemType::BackpackItemType_Armor, 502001, "Level 1 Helmet" },
    { SDK::EBackpackItemType::BackpackItemType_Armor, 502002, "Level 2 Helmet" },
    { SDK::EBackpackItemType::BackpackItemType_Armor, 502003, "Level 3 Helmet" },
    { SDK::EBackpackItemType::BackpackItemType_Armor, 503001, "Level 1 Vest" },
    { SDK::EBackpackItemType::BackpackItemType_Armor, 503002, "Level 2 Vest" },
    { SDK::EBackpackItemType::BackpackItemType_Armor, 503003, "Level 3 Vest" },
    { SDK::EBackpackItemType::BackpackItemType_Clothing, 403989, "Ghillie Suit" }
};

struct MapConsumableDef
{
    int32_t SpecificID;
    int32_t Count;
    const char* Name;
};

static const MapConsumableDef kMapConsumables[] = {
    { 601002, 1, "First Aid Kit" },
    { 601001, 5, "Bandages (x5)" },
    { 601006, 2, "Energy Drink (x2)" },
    { 601004, 1, "Painkiller" },
    { 601003, 1, "Med Kit" },
    { 601005, 1, "Adrenaline Syringe" },
    { 602001, 1, "Gas Can" }
};

struct MapAttachmentDef
{
    int32_t SpecificID;
    const char* Name;
};

static const MapAttachmentDef kMapAttachments[] = {
    { 201001, "Red Dot Sight" },
    { 201002, "Holographic Sight" },
    { 201003, "2x Aimpoint Scope" },
    { 201009, "3x Backlit Scope" },
    { 201004, "4x ACOG Scope" },
    { 201005, "6x Scope" },
    { 201006, "8x CQBSS Scope" },
    { 202001, "AR Compensator" },
    { 202004, "AR Suppressor" },
    { 202005, "SR Suppressor" },
    { 202006, "SMG Suppressor" },
    { 204003, "AR Ext. Quickdraw" },
    { 204006, "SR Ext. Quickdraw" },
    { 203001, "Vertical Foregrip" },
    { 203002, "Angled Foregrip" }
};

struct MapThrowableDef
{
    int32_t SpecificID;
    int32_t Count;
    const char* Name;
};

static const MapThrowableDef kMapThrowablesMelee[] = {
    { 106001, 2, "Frag Grenade (x2)" },
    { 106002, 2, "Smoke Grenade (x2)" },
    { 106003, 1, "Molotov Cocktail" },
    { 106004, 1, "Stun Grenade" }
};

struct MapLootSpawnerState
{
    bool bActive = false;
    std::vector<SDK::FVector> Spots;
    size_t CurrentIndex = 0;
    int TotalItemsSpawned = 0;
    int TotalSpotsProcessed = 0;
};
static MapLootSpawnerState g_MapLoot;

void ForceHighQualityForBuildingActors();

template <typename T, size_t N>
bool TryPick(const T(&items)[N], const T*& result)
{
    if constexpr (N == 0)
        return false;

    static thread_local std::mt19937 rng{ std::random_device{}() };

    std::uniform_int_distribution<size_t> dist(0, N - 1);
    result = &items[dist(rng)];
    return true;
}

void ProcessMapLootTick(SDK::UWorld* World)
{
    if (!g_MapLoot.bActive || !World) return;

    const size_t BATCH_SIZE = 50;
    size_t totalSpots = g_MapLoot.Spots.size();
    size_t endIndex = (std::min)(g_MapLoot.CurrentIndex + BATCH_SIZE, totalSpots);

    //for (size_t i = g_MapLoot.CurrentIndex; i < totalSpots; ++i)
    for (size_t i = g_MapLoot.CurrentIndex; i < endIndex; ++i)
    {
        __try
        {
            const SDK::FVector& SpotPos = g_MapLoot.Spots[i];
            g_MapLoot.TotalSpotsProcessed++;

            // 65% spawn chance per spot (matches authentic PUBG rate)
            //if ((rand() % 100) >= 65)
            //    continue;

            SDK::FVector SpawnPos = SpotPos;

            int roll = rand() % 100;

            if (roll < 32)
            {
                // Weapon + matching paired ammo
                int wIndex = rand() % (sizeof(kMapWeapons) / sizeof(kMapWeapons[0]));
                const auto& wDef = kMapWeapons[wIndex];

                if (SpawnLootItemAtLocation(World, SpawnPos, SDK::EBackpackItemType::BackpackItemType_Weapon, wDef.WeaponID, 1))
                {
                    g_MapLoot.TotalItemsSpawned++;

                    if (wDef.AmmoID > 0 && wDef.AmmoCount > 0)
                    {
                        SDK::FVector AmmoPos = SpawnPos;
                        AmmoPos.X += 25.0f;
                        AmmoPos.Y += 25.0f;
                        if (SpawnLootItemAtLocation(World, AmmoPos, SDK::EBackpackItemType::BackpackItemType_Ammo, wDef.AmmoID, wDef.AmmoCount))
                        {
                            g_MapLoot.TotalItemsSpawned++;
                        }
                    }
                }
            }
            else if (roll < 54)
            {
                // Standalone Ammo
                int aIndex = rand() % (sizeof(kMapAmmo) / sizeof(kMapAmmo[0]));
                const auto& aDef = kMapAmmo[aIndex];

                if (SpawnLootItemAtLocation(World, SpawnPos, SDK::EBackpackItemType::BackpackItemType_Ammo, aDef.AmmoID, aDef.Count))
                {
                    g_MapLoot.TotalItemsSpawned++;
                }
            }
            else if (roll < 70)
            {
                // Armor / Helmet / Bag / Ghillie
                int gIndex = rand() % (sizeof(kMapGear) / sizeof(kMapGear[0]));
                const auto& gDef = kMapGear[gIndex];

                if (SpawnLootItemAtLocation(World, SpawnPos, gDef.Type, gDef.SpecificID, 1))
                {
                    g_MapLoot.TotalItemsSpawned++;
                }
            }
            else if (roll < 84)
            {
                // Consumables / Meds
                int cIndex = rand() % (sizeof(kMapConsumables) / sizeof(kMapConsumables[0]));
                const auto& cDef = kMapConsumables[cIndex];

                if (SpawnLootItemAtLocation(World, SpawnPos, SDK::EBackpackItemType::BackpackItemType_Consumable, cDef.SpecificID, cDef.Count))
                {
                    g_MapLoot.TotalItemsSpawned++;
                }
            }
            else if (roll < 94)
            {
                // Attachments & Optics
                int atIndex = rand() % (sizeof(kMapAttachments) / sizeof(kMapAttachments[0]));
                const auto& atDef = kMapAttachments[atIndex];

                if (SpawnLootItemAtLocation(World, SpawnPos, SDK::EBackpackItemType::BackpackItemType_WeaponAttachment, atDef.SpecificID, 1))
                {
                    g_MapLoot.TotalItemsSpawned++;
                }
            }
            else
            {
                // Throwables & Melee
                int tIndex = rand() % (sizeof(kMapThrowablesMelee) / sizeof(kMapThrowablesMelee[0]));
                const auto& tDef = kMapThrowablesMelee[tIndex];

                if (SpawnLootItemAtLocation(World, SpawnPos, SDK::EBackpackItemType::BackpackItemType_Weapon, tDef.SpecificID, tDef.Count))
                {
                    g_MapLoot.TotalItemsSpawned++;
                }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
    }

    g_MapLoot.CurrentIndex = endIndex;

    // Log progress every 2,000 spots
    if (g_MapLoot.CurrentIndex % 2000 < BATCH_SIZE || g_MapLoot.CurrentIndex >= totalSpots)
    {
        printf("[MapLoot] Progress: %zu / %zu spots (%d items on ground)\n",
            g_MapLoot.CurrentIndex, totalSpots, g_MapLoot.TotalItemsSpawned);
    }

    if (g_MapLoot.CurrentIndex >= totalSpots)
    {
        g_MapLoot.bActive = false;
        printf("[MapLoot] ========================================================\n");
        printf("[MapLoot] *** FULL MAP LOOT SPAWNING COMPLETE! ***\n");
        printf("[MapLoot] Processed: %zu spots | Total Ground Items: %d\n",
            totalSpots, g_MapLoot.TotalItemsSpawned);
        printf("[MapLoot] All houses, cities, and military bases now have loot!\n");
        printf("[MapLoot] ========================================================\n");

        bIsLootFinished = true;
    }
}

static void SaveLootCoordinatesToFile(const std::vector<SDK::FVector>& Spots)
{
    if (Spots.size() < 10000)
    {
        printf("[MapLoot] Guard: Skipping save to disk because spot count (%zu) is too small (protecting full-map file).\n", Spots.size());
        return;
    }

    std::vector<std::string> targetDirs;
    targetDirs.push_back(""); // CWD

    uint32_t count = (uint32_t)Spots.size();

    for (const auto& dir : targetDirs)
    {
        std::string binPath = dir + "Erangel_Loot_Coordinates.bin";
        FILE* f = nullptr;
        fopen_s(&f, binPath.c_str(), "wb");
        if (f)
        {
            fwrite(&count, sizeof(uint32_t), 1, f);
            fwrite(Spots.data(), sizeof(SDK::FVector), count, f);
            fclose(f);
            printf("[MapLoot] Saved %u authentic interior spot coordinates to %s\n", count, binPath.c_str());
        }
    }
}

static void AddFloorSpots(
    const SDK::FVector& Center,
    const SDK::FVector& Fwd,
    const SDK::FVector& Right,
    float ExtentX,   // half-width  (along Right)
    float ExtentY,   // half-depth  (along Fwd)
    std::vector<SDK::FVector>& OutSpots)
{
    const float Spacing = 120.0f;  // cm between spots

    int colsX = std::max(1, (int)(ExtentX * 2.0f / Spacing));
    int colsY = std::max(1, (int)(ExtentY * 2.0f / Spacing));
    colsX = std::min(colsX, 6);
    colsY = std::min(colsY, 6);

    for (int iy = 0; iy < colsY; ++iy)
    {
        // -1..+1 range mapped across the depth
        float fy = (colsY == 1) ? 0.0f : ((float)iy / (colsY - 1) - 0.5f) * 2.0f;
        for (int ix = 0; ix < colsX; ++ix)
        {
            float fx = (colsX == 1) ? 0.0f : ((float)ix / (colsX - 1) - 0.5f) * 2.0f;

            SDK::FVector Spot = Center
                + (Fwd * (fy * ExtentY * 0.8f))  // 80% of half-extent (stay inside walls)
                + (Right * (fx * ExtentX * 0.8f));

            OutSpots.push_back(Spot);
        }
    }
}

// Compute the actual interior envelope from window positions (local space)
// Windows are on the walls, so their extent ≈ the building footprint.
bool GetInteriorEnvelope(
    SDK::AUAEHouseActor* House,
    const SDK::FVector& Loc,
    const SDK::FVector& Fwd,
    const SDK::FVector& Right,
    float& OutHalfX,   // half-width  along Right
    float& OutHalfY,   // half-depth  along Fwd
    SDK::FVector& OutCenterOffset) // offset from Loc to true center
{
    if (House->WindowList.Num() == 0)
    {
        // No windows — fall back to a conservative 40% of actor bounds
        SDK::FVector Origin, BoxExtent;
        House->GetActorBounds(false, &Origin, &BoxExtent);
        OutHalfX = BoxExtent.X * 0.4f;
        OutHalfY = BoxExtent.Y * 0.4f;
        OutCenterOffset = SDK::FVector(0, 0, 0);
        return true;
    }

    // Project all window positions into the house's local X/Y plane
    float minX = 1e9f, maxX = -1e9f;
    float minY = 1e9f, maxY = -1e9f;

    for (int w = 0; w < House->WindowList.Num(); ++w)
    {
        SDK::FVector WinLocal = House->WindowList[w].Transform.Translation - Loc;
        // Dot with Fwd/Right to get local coordinates
        float lx = WinLocal.X * Right.X + WinLocal.Y * Right.Y; // along Right
        float ly = WinLocal.X * Fwd.X + WinLocal.Y * Fwd.Y;   // along Fwd

        minX = (lx < minX) ? lx : minX;
        maxX = (lx > maxX) ? lx : maxX;
        minY = (ly < minY) ? ly : minY;
        maxY = (ly > maxY) ? ly : maxY;
    }

    // Inset by ~60 cm so spots don't sit on the wall
    const float WallInset = 60.0f;
    float width = (maxX - minX) - WallInset * 2.0f;
    float depth = (maxY - minY) - WallInset * 2.0f;

    if (width < 200.0f)  width = 200.0f;  // minimum 2 m interior
    if (depth < 200.0f)  depth = 200.0f;

    OutHalfX = width * 0.5f;
    OutHalfY = depth * 0.5f;

    // True center is the midpoint of the window envelope
    float cx = (minX + maxX) * 0.5f;
    float cy = (minY + maxY) * 0.5f;
    OutCenterOffset = (Right * cx) + (Fwd * cy);

    return true;
}

static bool HarvestAuthenticInteriorLootSpots(SDK::UWorld* World, std::vector<SDK::FVector>& OutSpots)
{
    OutSpots.clear();
    if (!SDK::UObject::GObjects) return false;

    int totalHousesScanned = 0;

    for (int i = 0; i < SDK::UObject::GObjects->Num(); ++i)
    {
        __try
        {
            SDK::UObject* Obj = SDK::UObject::GObjects->GetByIndex(i);
            if (!Obj || Obj->IsDefaultObject()) continue;

            if (!Obj->IsA(SDK::AUAEHouseActor::StaticClass())) continue;

            SDK::AUAEHouseActor* House = static_cast<SDK::AUAEHouseActor*>(Obj);
            totalHousesScanned++;

            SDK::FVector Loc = House->K2_GetActorLocation();
            if (Loc.X == 0.f && Loc.Y == 0.f) continue;

            SDK::FRotator Rot = House->K2_GetActorRotation();
            float YawRad = Rot.Yaw * (3.14159265358979323846f / 180.0f);
            SDK::FVector Fwd(cosf(YawRad), sinf(YawRad), 0.0f);
            SDK::FVector Right(-sinf(YawRad), cosf(YawRad), 0.0f);

            // Ground floor center: +25cm above house root pivot
            SDK::FVector Center = Loc;
            Center.Z = Loc.Z + 25.0f;

            // Check building height and window positions for 2nd and 3rd floors
            SDK::FVector Origin(0, 0, 0);
            SDK::FVector BoxExtent(0, 0, 0);
            House->GetActorBounds(false, &Origin, &BoxExtent);

            float HalfX, HalfY;
            SDK::FVector CenterOffset;
            GetInteriorEnvelope(House, Loc, Fwd, Right, HalfX, HalfY, CenterOffset);

            // Adjust the floor centers to the true interior center
            SDK::FVector TrueCenter = Loc + CenterOffset;
            TrueCenter.Z = Loc.Z + 25.0f;

            AddFloorSpots(TrueCenter, Fwd, Right, HalfX, HalfY, OutSpots);

            bool bHasFloor2 = false;
            bool bHasFloor3 = false;

            if (BoxExtent.Z >= 280.0f) bHasFloor2 = true;
            if (BoxExtent.Z >= 500.0f) bHasFloor3 = true;

            if (House->WindowList.Num() > 0)
            {
                for (int w = 0; w < House->WindowList.Num(); w++)
                {
                    float winZ = House->WindowList[w].Transform.Translation.Z - Loc.Z;
                    if (winZ > 240.0f) bHasFloor2 = true;
                    if (winZ > 540.0f) bHasFloor3 = true;
                }
            }

            // Floor 2 spots (~3.7m above ground floor)
            if (bHasFloor2)
            {
                SDK::FVector Floor2 = TrueCenter;
                Floor2.Z = Loc.Z + 370.0f;
                AddFloorSpots(Floor2, Fwd, Right, HalfX, HalfY, OutSpots);
            }

            // Floor 3 / Attic / Roof spots (~7.3m above ground floor)
            if (bHasFloor3)
            {
                SDK::FVector Floor3 = TrueCenter;
                Floor3.Z = Loc.Z + 730.0f;
                AddFloorSpots(Floor3, Fwd, Right, HalfX, HalfY, OutSpots);
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            continue;
        }
    }

    printf("[MapLoot] Harvested %d houses from memory (%zu multi-story interior spots).\n",
        totalHousesScanned, OutSpots.size());

    return !OutSpots.empty();
}

static bool LoadMapLootSpots(std::vector<SDK::FVector>& OutSpots)
{
    OutSpots.clear();

    /*
    std::vector<std::string> baseDirs;
    baseDirs.push_back(""); // Current working directory

    char exePath[MAX_PATH] = { 0 };
    if (GetModuleFileNameA(NULL, exePath, MAX_PATH))
    {
        char* lastSlash = strrchr(exePath, '\\');
        if (lastSlash)
        {
            *(lastSlash + 1) = '\0';
            baseDirs.push_back(std::string(exePath));
        }
    }

    if (g_hModule)
    {
        char dllPath[MAX_PATH] = { 0 };
        if (GetModuleFileNameA(g_hModule, dllPath, MAX_PATH))
        {
            char* lastSlash = strrchr(dllPath, '\\');
            if (lastSlash)
            {
                *(lastSlash + 1) = '\0';
                baseDirs.push_back(std::string(dllPath));
            }
        }
    }
    */

    if (HarvestAuthenticInteriorLootSpots(SDK::UWorld::GetWorld(), OutSpots))
    {
        printf("[MapLoot] Harvested %zu interior spots from memory!\n", OutSpots.size());
        //SaveLootCoordinatesToFile(OutSpots);
        return true;
    }

    return !OutSpots.empty();
}

void StartMapLootSpawning(SDK::UWorld* World)
{
    if (!World) World = SDK::UWorld::GetWorld();
    if (!World) return;

    if (g_MapLoot.bActive)
    {
        printf("[MapLoot] Map loot spawning already in progress (%zu / %zu spots)...\n",
            g_MapLoot.CurrentIndex, g_MapLoot.Spots.size());
        return;
    }

    if (g_MapLoot.TotalItemsSpawned > 0 || g_MapLoot.TotalSpotsProcessed > 0)
    {
        printf("[MapLoot] Map loot already spawned (%d items on ground). Skipping duplicate spawn!\n",
            g_MapLoot.TotalItemsSpawned);
        return;
    }

    printf("\n[MapLoot] ========================================================\n");
    printf("[MapLoot] Initializing Full Map Loot Spawner...\n");

    InitVisualWrapperClasses();

    if (g_MapLoot.Spots.empty())
    {
        LoadMapLootSpots(g_MapLoot.Spots);
    }

    if (g_MapLoot.Spots.empty())
    {
        printf("[MapLoot] ERROR: No loot spots found to spawn!\n");
        printf("[MapLoot] ========================================================\n");
        return;
    }

    g_MapLoot.CurrentIndex = 0;
    g_MapLoot.TotalItemsSpawned = 0;
    g_MapLoot.TotalSpotsProcessed = 0;
    g_MapLoot.bActive = true;
    //gRequestErangelLoot = true;

    printf("[MapLoot] Queued %zu spots across the map.\n", g_MapLoot.Spots.size());
    printf("[MapLoot] Spawning in batches of 25 spots/tick on game thread...\n");
    printf("[MapLoot] ========================================================\n");
}

static void SetupErangelLoot(SDK::UWorld* World)
{
    // 1. Configure replication settings with realistic distances
    SDK::UTslSettings* Settings = SDK::UTslSettings::GetTslSettings();
    if (!Settings) Settings = SDK::UTslSettings::GetDefaultObj();
    if (Settings)
    {
        Settings->RepDistance_Character = 100000.0f; // 1km
        Settings->RepDistance_Item = 20000.0f;      // 200m
        Settings->RepDistance_Weapon = 20000.0f;    // 200m
        Settings->RepDistance_Door = 30000.0f;      // 300m
        Settings->RepDistance_Window = 30000.0f;    // 300m
        Settings->RepDistance_Vehicle = 50000.0f;   // 500m
        Settings->RepVehicle_SpawnDistance = 50000.0f;
        printf("[Erangel-Loot] UTslSettings: Configured replication distances (Item=200m, Char=1km, Vehicle=500m).\n");
    }

    // 2. APickUpWrapperActor CDO replication settings
    SDK::APickUpWrapperActor* DefPickup = SDK::APickUpWrapperActor::GetDefaultObj();
    if (DefPickup)
    {
        DefPickup->bAlwaysRelevant = false;
        DefPickup->NetCullDistanceSquared = 15000.0f * 15000.0f; // 150m
        DefPickup->NetPriority = 1.5f;
    }

    // 3. GameMode setup
    SDK::AGameModeBase* BaseGM = SDK::UGameplayStatics::GetGameMode(World);
    if (!BaseGM)
    {
        printf("[Erangel-Loot] BaseGM is null!\n");
        return;
    }
    printf("[Erangel-Loot] GameMode: %s\n", BaseGM->GetName().c_str());

    if (BaseGM->IsA(SDK::AUAEGameMode::StaticClass()))
    {
        SDK::AUAEGameMode* UAEGM = (SDK::AUAEGameMode*)BaseGM;
        UAEGM->MaxAllowReplicatedCharacterCount = 101;
        UAEGM->MaxPlayerLimit = 101;
        UAEGM->bEnableDamage = true;
    }

    // 4. Start Full Map Loot Spawner across all 14,256 spots!
    StartMapLootSpawning(World);
}

void StartMapLootSpawning(SDK::UWorld* World);

void* ProcessEventHook(UObject* Obj, UFunction* Func, void* Func_Params)
{
    if (Obj && Func)
    {
        auto ObjName = Obj->GetName();
        auto FuncName = Func->GetName();

        if (FuncName == "OnGameModeEnterFlying")
        {

            //Sleep(12000);

            //gRequestErangelLoot = true;

            //StartMapLootSpawning(UWorld::GetWorld());

            /*
            SDK::UWorld* World = UWorld::GetWorld();

            SDK::AGameStateBase* GS = UGameplayStatics::GetGameState(World);

            UC::TArray<SDK::APlayerState*>& Players = GS->PlayerArray;

            ASTExtraPlayerController* STEPC = (ASTExtraPlayerController*)UGameplayStatics::GetPlayerController(UWorld::GetWorld(), 0);

            std::cerr << "Players: " << Players.Num() << std::endl;

            if (Players.Num() <= 100) {
                for (int i = 0; i < 100 - Players.Num() - 10; ++i) {
                    UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), L"localspawnai 1", nullptr);
                }
            }

            std::cerr << "Players: " << Players.Num() << std::endl;
            */


            /*
            if (FuncName == "K2_OnSetMatchState")
            {
                std::cerr << "New MatchState" << std::endl;

                auto Parmss = (Params::GameMode_K2_OnSetMatchState*)Func_Params;

                if (Parmss->NewState.ToString() == "WaitingPostMatch")
                {
                    Sleep(5000);
                    __fastfail(0);
                }
            }
            */

            if (!g_MapLoot.bActive && g_MapLoot.TotalItemsSpawned == 0)
            {
                printf("[Plane] >>> Match entering Airplane / Flying phase! Starting Full Map Loot Spawner! <<<\n");
                gRequestErangelLoot = true;
            }
        }

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

            std::cerr << "ID: " << shoot->GetWeaponID() << std::endl;

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

        if (FuncName == "ClientAdjustPosition" || FuncName == "ClientVeryShortAdjustPosition" ||
            FuncName == "ClientAdjustRootMotionPosition" || FuncName == "ClientAdjustRootMotionSourcePosition")
        {
            return nullptr;
        }

        if (FuncName == "ServerMove" || FuncName == "ServerMoveDual" || FuncName == "ServerMoveDualHybridRootMotion" || FuncName == "ServerMoveOld")
        {
            if (Obj->IsA(UCharacterMovementComponent::StaticClass()))
            {
                UCharacterMovementComponent* CMC = (UCharacterMovementComponent*)Obj;
                CMC->bIgnoreClientMovementErrorChecksAndCorrection = true;
                if (CMC->IsA(USTCharacterMovementComponent::StaticClass()))
                {
                    USTCharacterMovementComponent* STCMC = (USTCharacterMovementComponent*)CMC;
                    STCMC->bServerMoveCheckPassWall = false;
                    STCMC->AdjustRadiusScaleWhenPassWall = 0.f;
                    STCMC->RadiusScaleWhenCheckPassWall = 0.f;
                    STCMC->MaxPenetrationAdjustmentSize = 0.f;
                }
            }
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

        //CheckMovement(STEPC, Character);

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

void ForceHighQualityForBuildingActors()
{
    printf("[BuildingQuality] ForceHighQualityForBuildingActors starting...\n");
    fflush(stdout);

    __try
    {
        UWorld* World = UWorld::GetWorld();
        if (!World || !UObject::GObjects)
            return;

        auto PC = UGameplayStatics::GetPlayerController(World, 0);
        if (PC)
        {
            UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"r.HLOD 0"), PC);
            UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"r.StaticMeshStreaming 0"), PC);
            UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"r.StreamingSM.NeverStreamOut 1"), PC);
            UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"r.StaticMeshLODDistanceScale 0"), PC);
            UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"r.DistanceCullingFactor 0"), PC);
            UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"r.DisableLODFade 1"), PC);
            UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"foliage.ForceLOD 0"), PC);
        }

        UTslSettings* TslSettings = UTslSettings::GetTslSettings();
        if (!TslSettings)
            TslSettings = UTslSettings::GetDefaultObj();

        if (TslSettings)
        {
            TslSettings->RepDistance_Door = 30000.0f;
            TslSettings->RepDistance_Window = 30000.0f;
            TslSettings->CharacterCorrection_MaxSpeed = 100000.0f;
            TslSettings->CharacterCorrection_MaxSpeedPositionError = 100000.0f;
            TslSettings->CharacterCorrection_ZeroSpeedPositionError = 100000.0f;
        }

        int totalObjects = UObject::GObjects->Num();
        for (int i = 0; i < totalObjects; ++i)
        {
            __try
            {
                UObject* Obj = UObject::GObjects->GetByIndex(i);
                if (!Obj || Obj->IsDefaultObject())
                    continue;

                if (Obj->IsA(AUAEHouseActor::StaticClass()))
                {
                    AUAEHouseActor* House = (AUAEHouseActor*)Obj;
                    House->SetActorEnableCollision(true);
                    House->WindowHideDistanceSquared = FLT_MAX;
                    House->WindowLoadDistanceSquared = FLT_MAX;
                    House->WindowLoadDistanceSquaredOnServer = FLT_MAX;
                    House->bEnableWindow = true;

                }
                else if (Obj->IsA(AStaticMeshActorFM::StaticClass()))
                {
                    AStaticMeshActorFM* SMAFM = (AStaticMeshActorFM*)Obj;
                    SMAFM->SetActorEnableCollision(true);

                    if (SMAFM->InstancedMeshComponent)
                    {
                        SMAFM->InstancedMeshComponent->bDisableCollision = false;
                        SMAFM->InstancedMeshComponent->LDMaxDrawDistance = FLT_MAX;
                        SMAFM->InstancedMeshComponent->CachedMaxDrawDistance = FLT_MAX;
                        SMAFM->InstancedMeshComponent->bAllowCullDistanceVolume = false;
                    }
                }
                else if (Obj->IsA(AStaticMeshActor::StaticClass()))
                {
                    AStaticMeshActor* SMA = (AStaticMeshActor*)Obj;
                    SMA->SetActorEnableCollision(true);

                    if (SMA->StaticMeshComponent)
                    {
                        SMA->StaticMeshComponent->LDMaxDrawDistance = FLT_MAX;
                        SMA->StaticMeshComponent->CachedMaxDrawDistance = FLT_MAX;
                        SMA->StaticMeshComponent->bAllowCullDistanceVolume = false;
                    }
                }
                else if (Obj->IsA(APUBGDoor::StaticClass()))
                {
                    APUBGDoor* Door = (APUBGDoor*)Obj;
                    Door->SetActorEnableCollision(true);
                    if (Door->doorMesh)
                    {
                        Door->doorMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                        Door->doorMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
                        Door->doorMesh->SetCollisionObjectType(ECollisionChannel::ECC_WorldStatic);
                    }
                    if (Door->doorMeshEx)
                    {
                        Door->doorMeshEx->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                        Door->doorMeshEx->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
                        Door->doorMeshEx->SetCollisionObjectType(ECollisionChannel::ECC_WorldStatic);
                    }
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                continue;
            }
        }

        printf("[BuildingQuality] ForceHighQualityForBuildingActors completed successfully.\n");
        fflush(stdout);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        printf("[BuildingQuality] SEH Exception caught in ForceHighQualityForBuildingActors.\n");
        fflush(stdout);
    }
}

__int64 (*UpdateStreamingStateO)(SDK::UWorldComposition* a1);
__int64 UpdateStreamingStateHook(SDK::UWorldComposition* a1)
{
    __try
    {
        //SafeProcessPlayers();

        SDK::UWorld* World = nullptr;
        if (GWorld && !IsBadReadPtr(GWorld, 8))
        {
            World = GWorld;
        }
        else if (a1)
        {
            auto OuterObj = a1->GetOuter();
            if (OuterObj && OuterObj->IsA(SDK::UWorld::StaticClass()))
            {
                World = (SDK::UWorld*)OuterObj;
                GWorld = World;
            }
        }


        // Run building setup only once on load or on explicit hotkey request (not every 10 seconds)
        static bool gBuildingInitDone = false;
        if (!gBuildingInitDone && World)
        {
            gBuildingInitDone = true;
            ForceHighQualityForBuildingActors();
        }
        if (gRequestBuildingRefresh)
        {
            gRequestBuildingRefresh = false;
            ForceHighQualityForBuildingActors();
        }

        if (gRequestErangelLoot && World)
        {
            gRequestErangelLoot = false;
            SetupErangelLoot(World);
        }

        if (World)
        {
            ProcessMapLootTick(World);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        std::cerr << "SEH Exception on UpdateStreamingState" << std::endl;
    }

    return UpdateStreamingStateO(a1);
}

// 0x7ff79f3e83e0
ENetMode(*T_UActorComponent_InternalGetNetMode)(SDK::UActorComponent*);
ENetMode hk_UActorComponent_InternalGetNetMode(SDK::UActorComponent* self)
{
    //void* caller = _ReturnAddress();

    return ENetMode::NM_DedicatedServer;
}

DWORD Hook_UActorComponent_InternalGetNetMode()
{

    // 0x7ff79f3e83e0
    {
        uintptr_t func_Addr = (uintptr_t(GetModuleHandle(0)) + 0x1E583E0); // 0x

        T_UActorComponent_InternalGetNetMode = decltype(T_UActorComponent_InternalGetNetMode)(func_Addr);
        MH_STATUS HookResult = MH_CreateHook((PVOID&)func_Addr, hk_UActorComponent_InternalGetNetMode, reinterpret_cast <LPVOID*> (&T_UActorComponent_InternalGetNetMode));
        const char* HookResultString = MH_StatusToString(HookResult);

        if (HookResult != MH_STATUS::MH_OK)
        {
            printf("\n Hook UActorComponent::InternalGetNetMode() CREATED FAILED WITH REASON : %s !\n", HookResultString);
        }
        else
        {
            if (MH_EnableHook((PVOID&)func_Addr) != MH_STATUS::MH_OK)
            {
                printf("\n UActorComponent::InternalGetNetMode() Hook ENABLE FAILED! \n");
            }
            else
            {
                printf("\n UActorComponent::InternalGetNetMode() Hook Enabled! \n");
            }
        }
    } // -------------------------------------------------------------------------------------------------------------------------



    return 0;
}


// FUN_7ff79da40180 =  ENetMode AActor::InternalGetNetMode(AActor *this)
ENetMode(*T_AActor_InternalGetNetMode)(SDK::AActor*);
ENetMode hk_AActor_InternalGetNetMode(SDK::AActor* self)
{
    //void* caller = _ReturnAddress();


    return ENetMode::NM_DedicatedServer;
}

DWORD Hook_AActor_InternalGetNetMode()
{
    // 0x7ff79da40180

    {
        uintptr_t func_Addr = (uintptr_t(GetModuleHandle(0)) + 0x4B0180); // 0x4B0180

        T_AActor_InternalGetNetMode = decltype(T_AActor_InternalGetNetMode)(func_Addr);
        MH_STATUS HookResult = MH_CreateHook((PVOID&)func_Addr, hk_AActor_InternalGetNetMode, reinterpret_cast <LPVOID*> (&T_AActor_InternalGetNetMode));
        const char* HookResultString = MH_StatusToString(HookResult);

        if (HookResult != MH_STATUS::MH_OK)
        {
            printf("\n Hook AActor::InternalGetNetMode() CREATED FAILED WITH REASON : %s !\n", HookResultString);
        }
        else
        {
            if (MH_EnableHook((PVOID&)func_Addr) != MH_STATUS::MH_OK)
            {
                printf("\n AActor::InternalGetNetMode() Hook ENABLE FAILED! \n");
            }
            else
            {
                printf("\n AActor::InternalGetNetMode() Hook Enabled! \n");
            }
        }
    } // -------------------------------------------------------------------------------------------------------------------------



    return 0;
}

using IsDistanceDependentLevelFn = bool(__fastcall*)(UWorldComposition* __this, FName a2);
IsDistanceDependentLevelFn oIsDistanceDependentLevel = nullptr;

bool __fastcall hkIsDistanceDependentLevel(UWorldComposition* __this, FName a2)
{
    return false;
}

DWORD HookDistanceStreaming()
{
    uintptr_t func_Addr = (uintptr_t(GetModuleHandle(0)) + 0xC5A140); // 0x4B0180

    oIsDistanceDependentLevel = decltype(oIsDistanceDependentLevel)(func_Addr);
    MH_STATUS HookResult = MH_CreateHook((PVOID&)func_Addr, hkIsDistanceDependentLevel, reinterpret_cast <LPVOID*> (&oIsDistanceDependentLevel));
    const char* HookResultString = MH_StatusToString(HookResult);

    if (HookResult != MH_STATUS::MH_OK)
    {
        printf("\n Hook IsDistanceDependentLevel CREATED FAILED WITH REASON : %s !\n", HookResultString);
    }
    else
    {
        if (MH_EnableHook((PVOID&)func_Addr) != MH_STATUS::MH_OK)
        {
            printf("\n IsDistanceDependentLevel Hook ENABLE FAILED! \n");
        }
        else
        {
            printf("\n IsDistanceDependentLevel Hook Enabled! \n");
        }
    }

    return 0;
}

using IsTileVisibleFn = bool(__fastcall*)(void* WorldComposition, int TileIndex);

IsTileVisibleFn oIsTileVisible = nullptr;

bool __fastcall hkIsTileVisible(void* WorldComposition, int TileIndex)
{
    return true;
}

DWORD HookTileVisibility()
{
    uintptr_t func_Addr = (uintptr_t(GetModuleHandle(0)) + 0x193BEB0);

    oIsTileVisible = decltype(oIsTileVisible)(func_Addr);
    MH_STATUS HookResult = MH_CreateHook((PVOID&)func_Addr, hkIsTileVisible, reinterpret_cast <LPVOID*> (&oIsTileVisible));
    const char* HookResultString = MH_StatusToString(HookResult);

    if (HookResult != MH_STATUS::MH_OK)
    {
        printf("\n Hook hkIsTileVisible CREATED FAILED WITH REASON : %s !\n", HookResultString);
    }
    else
    {
        if (MH_EnableHook((PVOID&)func_Addr) != MH_STATUS::MH_OK)
        {
            printf("\n hkIsTileVisible Hook ENABLE FAILED! \n");
        }
        else
        {
            printf("\n hkIsTileVisible Hook Enabled! \n");
        }
    }

    return 0;
}

using hk769DE0Fn = __int64(__fastcall*)(void* PC,
    void* LevelName,
    bool Loaded,
    bool Visible,
    bool Block,
    int LOD);
hk769DE0Fn o769DE0 = nullptr;
__int64 __fastcall hk769DE0(
    UObject* PC,
    void* LevelName,
    bool Loaded,
    bool Visible,
    bool Block,
    int LOD)
{
    std::cerr << "769DE0" << std::endl; std::cerr << PC->GetName() << std::endl;
    std::cerr << "LOD: " << LOD << std::endl;
    return o769DE0(PC, LevelName, true, true, true, 0);
}

DWORD Hook769DE0()
{
    uintptr_t func_Addr = (uintptr_t(GetModuleHandle(0)) + 0x769DE0);

    o769DE0 = decltype(o769DE0)(func_Addr);
    MH_STATUS HookResult = MH_CreateHook((PVOID&)func_Addr, hk769DE0, reinterpret_cast <LPVOID*> (&o769DE0));
    const char* HookResultString = MH_StatusToString(HookResult);

    if (HookResult != MH_STATUS::MH_OK)
    {
        printf("\n Hook 769DE0 CREATED FAILED WITH REASON : %s !\n", HookResultString);
    }
    else
    {
        if (MH_EnableHook((PVOID&)func_Addr) != MH_STATUS::MH_OK)
        {
            printf("\n 769DE0 Hook ENABLE FAILED! \n");
        }
        else
        {
            printf("\n 769DE0 Hook Enabled! \n");
        }
    }

    return 0;
}

using hkC51650Fn = bool(__fastcall*)(__int64 a1, __int64 a2, float a3, float* a4);
hkC51650Fn oC51650 = nullptr;
bool __fastcall hkC51650(__int64 a1, __int64 a2, float a3, float* a4)
{
    //std::cerr << "C51650" << std::endl;
    return true;
    //return oC51650(PC, LevelName, true, true, true, LOD);
}

DWORD HookC51650()
{
    uintptr_t func_Addr = (uintptr_t(GetModuleHandle(0)) + 0xC51650);

    oC51650 = decltype(oC51650)(func_Addr);
    MH_STATUS HookResult = MH_CreateHook((PVOID&)func_Addr, hkC51650, reinterpret_cast <LPVOID*> (&oC51650));
    const char* HookResultString = MH_StatusToString(HookResult);

    if (HookResult != MH_STATUS::MH_OK)
    {
        printf("\n Hook C51650 CREATED FAILED WITH REASON : %s !\n", HookResultString);
    }
    else
    {
        if (MH_EnableHook((PVOID&)func_Addr) != MH_STATUS::MH_OK)
        {
            printf("\n C51650 Hook ENABLE FAILED! \n");
        }
        else
        {
            printf("\n C51650 Hook Enabled! \n");
        }
    }

    return 0;
}

using hk10BE040Fn = bool(__fastcall*)(__int64 a1, unsigned int* a2, unsigned int* a3);
hk10BE040Fn o10BE040 = nullptr;
bool __fastcall hk10BE040(__int64 a1, unsigned int* a2, unsigned int* a3)
{
    //std::cerr << "10BE040" << std::endl;
    return true;
    //return oC51650(PC, LevelName, true, true, true, LOD);
}

DWORD Hook10BE040()
{
    uintptr_t func_Addr = (uintptr_t(GetModuleHandle(0)) + 0x10BE040);

    o10BE040 = decltype(o10BE040)(func_Addr);
    MH_STATUS HookResult = MH_CreateHook((PVOID&)func_Addr, hk10BE040, reinterpret_cast <LPVOID*> (&o10BE040));
    const char* HookResultString = MH_StatusToString(HookResult);

    if (HookResult != MH_STATUS::MH_OK)
    {
        printf("\n Hook 10BE040 CREATED FAILED WITH REASON : %s !\n", HookResultString);
    }
    else
    {
        if (MH_EnableHook((PVOID&)func_Addr) != MH_STATUS::MH_OK)
        {
            printf("\n 10BE040 Hook ENABLE FAILED! \n");
        }
        else
        {
            printf("\n 10BE040 Hook Enabled! \n");
        }
    }

    return 0;
}

using hkGetMaxDrawDistanceFn = float(__fastcall*)(__int64 proxy);
hkGetMaxDrawDistanceFn oGetMaxDrawDistance = nullptr;
float hkGetMaxDrawDistance(__int64 proxy)
{
    return FLT_MAX;
}

DWORD HookGetMaxDrawDistance()
{
    uintptr_t func_Addr = (uintptr_t(GetModuleHandle(0)) + 0x1B405D0);

    oGetMaxDrawDistance = decltype(oGetMaxDrawDistance)(func_Addr);
    MH_STATUS HookResult = MH_CreateHook((PVOID&)func_Addr, hkGetMaxDrawDistance, reinterpret_cast <LPVOID*> (&oGetMaxDrawDistance));
    const char* HookResultString = MH_StatusToString(HookResult);

    if (HookResult != MH_STATUS::MH_OK)
    {
        printf("\n Hook GetMaxDrawDistance CREATED FAILED WITH REASON : %s !\n", HookResultString);
    }
    else
    {
        if (MH_EnableHook((PVOID&)func_Addr) != MH_STATUS::MH_OK)
        {
            printf("\n GetMaxDrawDistance Hook ENABLE FAILED! \n");
        }
        else
        {
            printf("\n GetMaxDrawDistance Hook Enabled! \n");
        }
    }

    return 0;
}

using hk1B40590Fn = float(__fastcall*)(__int64 proxy);
hk1B40590Fn o1B40590 = nullptr;
float hk1B40590(__int64 proxy)
{
    return FLT_MAX;
}

DWORD Hook1B40590()
{
    uintptr_t func_Addr = (uintptr_t(GetModuleHandle(0)) + 0x1B40590);

    o1B40590 = decltype(o1B40590)(func_Addr);
    MH_STATUS HookResult = MH_CreateHook((PVOID&)func_Addr, hk1B40590, reinterpret_cast <LPVOID*> (&o1B40590));
    const char* HookResultString = MH_StatusToString(HookResult);

    if (HookResult != MH_STATUS::MH_OK)
    {
        printf("\n Hook 1B40590 CREATED FAILED WITH REASON : %s !\n", HookResultString);
    }
    else
    {
        if (MH_EnableHook((PVOID&)func_Addr) != MH_STATUS::MH_OK)
        {
            printf("\n 1B40590 Hook ENABLE FAILED! \n");
        }
        else
        {
            printf("\n 1B40590 Hook Enabled! \n");
        }
    }

    return 0;
}

using hk4C8A50Fn = void(__fastcall*)(__int64* a1, int a2, __int64 a3);
hk4C8A50Fn o4C8A50 = nullptr;
void hk4C8A50(__int64* a1, int a2, __int64 a3)
{
    return;
}

DWORD Hook4C8A50()
{
    uintptr_t func_Addr = (uintptr_t(GetModuleHandle(0)) + 0x4C8A50);

    o4C8A50 = decltype(o4C8A50)(func_Addr);
    MH_STATUS HookResult = MH_CreateHook((PVOID&)func_Addr, hk4C8A50, reinterpret_cast <LPVOID*> (&o4C8A50));
    const char* HookResultString = MH_StatusToString(HookResult);

    if (HookResult != MH_STATUS::MH_OK)
    {
        printf("\n Hook 4C8A50 CREATED FAILED WITH REASON : %s !\n", HookResultString);
    }
    else
    {
        if (MH_EnableHook((PVOID&)func_Addr) != MH_STATUS::MH_OK)
        {
            printf("\n 4C8A50 Hook ENABLE FAILED! \n");
        }
        else
        {
            printf("\n 4C8A50 Hook Enabled! \n");
        }
    }

    return 0;
}

using hk8D5220Fn = void(__fastcall*)(__int64 a1);
hk8D5220Fn o8D5220 = nullptr;
__int64 hk8D5220(__int64 a1)
{
    return a1;
}

DWORD Hook8D5220()
{
    uintptr_t func_Addr = (uintptr_t(GetModuleHandle(0)) + 0x8D5220);

    o8D5220 = decltype(o8D5220)(func_Addr);
    MH_STATUS HookResult = MH_CreateHook((PVOID&)func_Addr, hk8D5220, reinterpret_cast <LPVOID*> (&o8D5220));
    const char* HookResultString = MH_StatusToString(HookResult);

    if (HookResult != MH_STATUS::MH_OK)
    {
        printf("\n Hook 8D5220 CREATED FAILED WITH REASON : %s !\n", HookResultString);
    }
    else
    {
        if (MH_EnableHook((PVOID&)func_Addr) != MH_STATUS::MH_OK)
        {
            printf("\n 8D5220 Hook ENABLE FAILED! \n");
        }
        else
        {
            printf("\n 8D5220 Hook Enabled! \n");
        }
    }

    return 0;
}

struct FDistanceVisibleLevel
{
    // 0x00
    int32 TileIdx;

    // 0x04
    int32 Pad[0x04];

    // 0x08
    ULevelStreaming* StreamingLevel;

    // 0x10
    int32 LODIndex;

    // 0x14
    int32 Pad2[0x04];

    // 0x18
    float DistanceSquared;

    // 0x1C
    float something;
};

struct TArrayRaw
{
    uint8_t* Data;  // +0x00
    int32_t  Num;   // +0x08
    int32_t  Max;   // +0x0C
};

inline void GrowArrayRaw(void* Array)
{
    reinterpret_cast<uint64_t(__fastcall*)(void*)>(InSDKUtils::GetImageBase() + 0x2B26D0)(Array);
}

using hkC53BD0Fn = __int64(__fastcall*)(UWorldComposition* a1, const FVector* InLocations,
    int32 Num,
    TArray<FDistanceVisibleLevel>& OutVisibleLevels,
    TArray<FDistanceVisibleLevel>& OutHiddenLevels); //GetDistanceVisibleLevels
hkC53BD0Fn oC53BD0 = nullptr;
__int64 hkC53BD0(
    UWorldComposition* WorldComposition,
    const FVector* InLocations,
    int32 Num,
    TArray<FDistanceVisibleLevel>& Visible,
    TArray<FDistanceVisibleLevel>& Hidden)
{
    const auto result =
        oC53BD0(WorldComposition, InLocations, Num, Visible, Hidden);

    auto* V = reinterpret_cast<TArrayRaw*>(&Visible);
    auto* H = reinterpret_cast<TArrayRaw*>(&Hidden);

    const int32 oldVisibleNum = V->Num;
    const int32 hiddenNum = H->Num;

    if (hiddenNum > 0)
    {
        const int32 required = oldVisibleNum + hiddenNum;

        if (required > V->Max)
        {
            V->Num = required;
            GrowArrayRaw(V);
            V->Num = oldVisibleNum;
        }

        uint8_t* visibleData = V->Data;
        uint8_t* hiddenData = H->Data;

        std::memcpy(
            visibleData + (size_t)oldVisibleNum * 0x20,
            hiddenData,
            (size_t)hiddenNum * 0x20);

        V->Num = required;
        H->Num = 0;
    }

    return result;
}

DWORD HookC53BD0()
{
    uintptr_t func_Addr = (uintptr_t(GetModuleHandle(0)) + 0xC53BD0);

    oC53BD0 = decltype(oC53BD0)(func_Addr);
    MH_STATUS HookResult = MH_CreateHook((PVOID&)func_Addr, hkC53BD0, reinterpret_cast <LPVOID*> (&oC53BD0));
    const char* HookResultString = MH_StatusToString(HookResult);

    if (HookResult != MH_STATUS::MH_OK)
    {
        printf("\n Hook C53BD0 CREATED FAILED WITH REASON : %s !\n", HookResultString);
    }
    else
    {
        if (MH_EnableHook((PVOID&)func_Addr) != MH_STATUS::MH_OK)
        {
            printf("\n C53BD0 Hook ENABLE FAILED! \n");
        }
        else
        {
            printf("\n C53BD0 Hook Enabled! \n");
        }
    }

    return 0;
}

using SetLoadedLevelFn = __int64(__fastcall*)(__int64 StreamingLevel, __int64 NewLevel);
SetLoadedLevelFn o1389470;

__int64 __fastcall hk1389470(__int64 StreamingLevel, __int64 NewLevel)
{
    // Ignore unload requests.
    if (NewLevel == 0)
        return 0;

    // Preserve normal load behavior.
    return o1389470(StreamingLevel, NewLevel);
}

DWORD Hook1389470()
{
    uintptr_t func_Addr = (uintptr_t(GetModuleHandle(0)) + 0x1389470);

    o1389470 = decltype(o1389470)(func_Addr);
    MH_STATUS HookResult = MH_CreateHook((PVOID&)func_Addr, hk1389470, reinterpret_cast <LPVOID*> (&o1389470));
    const char* HookResultString = MH_StatusToString(HookResult);

    if (HookResult != MH_STATUS::MH_OK)
    {
        printf("\n Hook 1389470 CREATED FAILED WITH REASON : %s !\n", HookResultString);
    }
    else
    {
        if (MH_EnableHook((PVOID&)func_Addr) != MH_STATUS::MH_OK)
        {
            printf("\n 1389470 Hook ENABLE FAILED! \n");
        }
        else
        {
            printf("\n 1389470 Hook Enabled! \n");
        }
    }

    return 0;
}

using RemoveFromWorldFn = char(__fastcall*)(__int64 World, __int64 Level, char bSomething);
RemoveFromWorldFn oC60330;
char __fastcall hkC60330(__int64 World, __int64 Level, char bSomething)
{
    return 1;
}

DWORD HookC60330()
{
    uintptr_t func_Addr = (uintptr_t(GetModuleHandle(0)) + 0xC60330);

    oC60330 = decltype(oC60330)(func_Addr);
    MH_STATUS HookResult = MH_CreateHook((PVOID&)func_Addr, hkC60330, reinterpret_cast <LPVOID*> (&oC60330));
    const char* HookResultString = MH_StatusToString(HookResult);

    if (HookResult != MH_STATUS::MH_OK)
    {
        printf("\n Hook C60330 CREATED FAILED WITH REASON : %s !\n", HookResultString);
    }
    else
    {
        if (MH_EnableHook((PVOID&)func_Addr) != MH_STATUS::MH_OK)
        {
            printf("\n C60330 Hook ENABLE FAILED! \n");
        }
        else
        {
            printf("\n C60330 Hook Enabled! \n");
        }
    }

    return 0;
}

using C4C1E0Fn = char(__fastcall*)(__int64 WC,
    __int64 World,
    int Tile,
    uint8_t Visible,
    uint8_t Loaded,
    uint8_t Block,
    int LOD);
C4C1E0Fn oC4C1E0;
char __fastcall hkC4C1E0(
    __int64 WC,
    __int64 World,
    int Tile,
    uint8_t Visible,
    uint8_t Loaded,
    uint8_t Block,
    int LOD)
{
    auto ret = oC4C1E0(WC, World, Tile, 1, 1, 1, LOD);

    auto streaming = *(__int64**)(WC + 160);
    auto level = streaming[Tile];

    *(uint32_t*)(level + 196) |= 0x20 | 0x40 | 0x100;
    *(uint8_t*)(level + 208) &= ~0x02;

    return ret;
}

DWORD HookC4C1E0()
{
    uintptr_t func_Addr = (uintptr_t(GetModuleHandle(0)) + 0xC4C1E0);

    oC4C1E0 = decltype(oC4C1E0)(func_Addr);
    MH_STATUS HookResult = MH_CreateHook((PVOID&)func_Addr, hkC4C1E0, reinterpret_cast <LPVOID*> (&oC4C1E0));
    const char* HookResultString = MH_StatusToString(HookResult);

    if (HookResult != MH_STATUS::MH_OK)
    {
        printf("\n Hook C4C1E0 CREATED FAILED WITH REASON : %s !\n", HookResultString);
    }
    else
    {
        if (MH_EnableHook((PVOID&)func_Addr) != MH_STATUS::MH_OK)
        {
            printf("\n C4C1E0 Hook ENABLE FAILED! \n");
        }
        else
        {
            printf("\n C4C1E0 Hook Enabled! \n");
        }
    }

    return 0;
}

using F5FFD0Fn = __int64(__fastcall*)(__int64 a1);
F5FFD0Fn oF5FFD0;
char __fastcall hkF5FFD0(__int64 a1)
{
    std::cerr << "UBaseGeneratorComponent::CheckTileLevelsVisible CALLED" << std::endl;
    return oF5FFD0(a1);
}

DWORD HookF5FFD0()
{
    uintptr_t func_Addr = (uintptr_t(GetModuleHandle(0)) + 0xF5FFD0);

    oF5FFD0 = decltype(oF5FFD0)(func_Addr);
    MH_STATUS HookResult = MH_CreateHook((PVOID&)func_Addr, hkF5FFD0, reinterpret_cast <LPVOID*> (&oF5FFD0));
    const char* HookResultString = MH_StatusToString(HookResult);

    if (HookResult != MH_STATUS::MH_OK)
    {
        printf("\n Hook F5FFD0 CREATED FAILED WITH REASON : %s !\n", HookResultString);
    }
    else
    {
        if (MH_EnableHook((PVOID&)func_Addr) != MH_STATUS::MH_OK)
        {
            printf("\n F5FFD0 Hook ENABLE FAILED! \n");
        }
        else
        {
            printf("\n F5FFD0 Hook Enabled! \n");
        }
    }

    return 0;
}

using F6BFD0Fn = __int64(__fastcall*)(__int64 a1);
F6BFD0Fn oF6BFD0;
char __fastcall hkF6BFD0(__int64 a1)
{
    std::cerr << "UItemGeneratorComponent::InitCatetorys CALLED" << std::endl;
    return oF6BFD0(a1);
}

DWORD HookF6BFD0()
{
    uintptr_t func_Addr = (uintptr_t(GetModuleHandle(0)) + 0xF6BFD0);

    oF6BFD0 = decltype(oF6BFD0)(func_Addr);
    MH_STATUS HookResult = MH_CreateHook((PVOID&)func_Addr, hkF6BFD0, reinterpret_cast <LPVOID*> (&oF6BFD0));
    const char* HookResultString = MH_StatusToString(HookResult);

    if (HookResult != MH_STATUS::MH_OK)
    {
        printf("\n Hook F6BFD0 CREATED FAILED WITH REASON : %s !\n", HookResultString);
    }
    else
    {
        if (MH_EnableHook((PVOID&)func_Addr) != MH_STATUS::MH_OK)
        {
            printf("\n F6BFD0 Hook ENABLE FAILED! \n");
        }
        else
        {
            printf("\n F6BFD0 Hook Enabled! \n");
        }
    }

    return 0;
}

using F6CDD0Fn = __int64(__fastcall*)(__int64 a1);
F6CDD0Fn oF6CDD0;
char __fastcall hkF6CDD0(__int64 a1)
{
    std::cerr << "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABCDEFG" << std::endl;
    return oF6CDD0(a1);
}

DWORD HookF6CDD0()
{
    uintptr_t func_Addr = (uintptr_t(GetModuleHandle(0)) + 0xF6CDD0);

    oF6CDD0 = decltype(oF6CDD0)(func_Addr);
    MH_STATUS HookResult = MH_CreateHook((PVOID&)func_Addr, hkF6CDD0, reinterpret_cast <LPVOID*> (&oF6CDD0));
    const char* HookResultString = MH_StatusToString(HookResult);

    if (HookResult != MH_STATUS::MH_OK)
    {
        printf("\n Hook F6CDD0 CREATED FAILED WITH REASON : %s !\n", HookResultString);
    }
    else
    {
        if (MH_EnableHook((PVOID&)func_Addr) != MH_STATUS::MH_OK)
        {
            printf("\n F6CDD0 Hook ENABLE FAILED! \n");
        }
        else
        {
            printf("\n F6CDD0 Hook Enabled! \n");
        }
    }

    return 0;
}

void hooksagain()
{
    Hook_AActor_InternalGetNetMode();
    Hook_UActorComponent_InternalGetNetMode();

    HookDistanceStreaming();
    HookTileVisibility();

    HookGetMaxDrawDistance();

    Hook1B40590();
    Hook769DE0();
    HookC51650();
    Hook10BE040();
    Hook4C8A50();
    Hook8D5220();
    HookC53BD0();
    Hook1389470();
    HookC60330();
    HookC4C1E0();
    HookF5FFD0();
    HookF6BFD0();
    HookF6CDD0();

    uintptr_t GetNetMode = (uintptr_t(GetModuleHandle(0)) + 0xCFD600);
    uintptr_t GetNetMode2 = (uintptr_t(GetModuleHandle(0)) + 0x4B0180);
    uintptr_t UpdateStreamingState = (uintptr_t(GetModuleHandle(0)) + 0xC6A560);

    MH_STATUS netmodeHookResult = MH_CreateHook((PVOID&)GetNetMode, GetNetModeHook, reinterpret_cast <LPVOID*> (&GetNetModeO));
    const char* netmodeHookResultString = MH_StatusToString(netmodeHookResult);

    
    MH_STATUS UpdateStreamingStateHookResult = MH_CreateHook((PVOID&)UpdateStreamingState, UpdateStreamingStateHook, reinterpret_cast<LPVOID*>(&UpdateStreamingStateO));
    const char* UpdateStreamingStateHookResultString = MH_StatusToString(UpdateStreamingStateHookResult);

    if (UpdateStreamingStateHookResult != MH_STATUS::MH_OK)
    {
        printf("\n Hook ProcessLevelStreamingVolumes CREATED FAILED WITH REASON : %s !\n", UpdateStreamingStateHookResultString);
    }
    else
    {
        if (MH_EnableHook((PVOID&)UpdateStreamingState) != MH_STATUS::MH_OK)
        {
            printf("\n ProcessLevelStreamingVolumes Hook ENABLE FAILED! \n");
        }
        else
        {
            std::cout << "ProcessLevelStreamingVolumes Hook ENABLED" << std::endl;
        }
    }
    

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

enum class GameMap
{
    Unknown,
    Bodie,
    Periverka,
    Erangel,
    Miramar,
    Sanhok,
    Vikendi,
    Training
};

GameMap GetLaunchMap()
{
    const wchar_t* commandLine = GetCommandLineW();

    if (!commandLine)
        return GameMap::Unknown;

    std::wstring cmd(commandLine);

    // Convert to lowercase so -map=BODIE also works.
    std::transform(
        cmd.begin(),
        cmd.end(),
        cmd.begin(),
        [](wchar_t c) {
            return static_cast<wchar_t>(towlower(c));
        }
    );

    const std::wstring prefix = L"-map=";

    size_t pos = cmd.find(prefix);

    if (pos == std::wstring::npos)
    {
        if (cmd.find(L"-erangel") != std::wstring::npos)
            return GameMap::Erangel;
        if (cmd.find(L"-miramar") != std::wstring::npos)
            return GameMap::Miramar;
        if (cmd.find(L"-sanhok") != std::wstring::npos)
            return GameMap::Sanhok;
        if (cmd.find(L"-vikendi") != std::wstring::npos)
            return GameMap::Vikendi;
        if (cmd.find(L"-bodie") != std::wstring::npos)
            return GameMap::Bodie;
        if (cmd.find(L"-periverka") != std::wstring::npos)
            return GameMap::Periverka;
        if (cmd.find(L"-training") != std::wstring::npos)
            return GameMap::Training;

        return GameMap::Unknown;
    }

    pos += prefix.length();

    size_t end = cmd.find_first_of(L" \t\r\n", pos);

    std::wstring mapName;

    if (end == std::wstring::npos)
        mapName = cmd.substr(pos);
    else
        mapName = cmd.substr(pos, end - pos);

    if (mapName == L"bodie")
        return GameMap::Bodie;

    if (mapName == L"periverka")
        return GameMap::Periverka;

    if (mapName == L"erangel")
        return GameMap::Erangel;

    if (mapName == L"miramar")
        return GameMap::Miramar;

    if (mapName == L"sanhok")
        return GameMap::Sanhok;

    if (mapName == L"vikendi")
        return GameMap::Vikendi;

    if (mapName == L"training")
        return GameMap::Training;

    return GameMap::Unknown;
}

struct FProp
{
    SDK::UProperty* Property = nullptr;
    std::string Type;
    int32 Off = 0;
    int32 ByteOff = 0;
    uint8 Mask = 0xFF;

    bool Ok() const
    {
        return Property != nullptr;
    }
};

static FProp ObjProp(SDK::UObject* O, const char* Name)
{
    if (!O || !Name)
        return {};

    static std::unordered_map<std::string, FProp> Cache;
    static std::mutex CacheMutex;

    SDK::UClass* Cls = O->GetClass();
    if (!Cls)
        return {};

    std::string Key = Cls->GetName();
    Key += "::";
    Key += Name;

    {
        std::lock_guard<std::mutex> Lock(CacheMutex);
        auto It = Cache.find(Key);
        if (It != Cache.end())
            return It->second;
    }

    FProp Result{};

    for (SDK::UStruct* S = Cls; S; S = S->Super)
    {
        for (SDK::UField* Field = S->Children;
            Field;
            Field = Field->Next)
        {
            if (!Field->IsA(SDK::UProperty::StaticClass()))
                continue;

            auto* P = static_cast<SDK::UProperty*>(Field);

            if (P->GetName() != Name)
                continue;

            Result.Property = P;
            Result.Off = P->Offset;

            if (P->IsA(SDK::UBoolProperty::StaticClass()))
            {
                auto* BP = static_cast<SDK::UBoolProperty*>(P);
                Result.Type = "BoolProperty";
                Result.ByteOff = BP->ByteOffset;
                Result.Mask = BP->FieldMask;
            }
            else
            {
                Result.Type = "OtherProperty";
            }

            break;
        }

        if (Result.Ok())
            break;
    }

    if (Result.Ok())
    {
        std::lock_guard<std::mutex> Lock(CacheMutex);
        Cache.emplace(Key, Result);
    }

    return Result;
}

bool WriteBool(SDK::UObject* O, const char* Name, bool V)
{
    FProp P = ObjProp(O, Name);

    if (!O || !P.Ok() || P.Type != "BoolProperty")
        return false;

    auto* B = reinterpret_cast<uint8*>(
        reinterpret_cast<uint8*>(O) + P.Off + P.ByteOff
        );

    if (P.Mask == 0xFF)
        *B = V ? 1 : 0;
    else
        *B = V ? (*B | P.Mask) : (*B & ~P.Mask);

    return true;
}

void zaebal(UWorld* World)
{
    for (ULevelStreaming* L : World->StreamingLevels) {
        if (!L) continue;

        std::cerr << L->PackageName.ToString() << std::endl;

        //if (L->PackageName.ToString().contains("eather")) continue;
        //read bShouldBeLoaded / bShouldBeVisible / bDisableDistanceStreaming; if all set, continue;
        WriteBool(L, "bDisableDistanceStreaming", true);
        WriteBool(L, "bShouldBeLoaded", true);
        WriteBool(L, "bShouldBeVisible", true);
    }

    for (ULevelStreaming* LS : World->WorldComposition->TilesStreaming)
    {
        if (!LS) continue;

        std::cerr << LS->PackageName.ToString() << std::endl;

        UGameplayStatics::LoadStreamLevel(UWorld::GetWorld(), LS->PackageNameToLoad, 1, 0, FLatentActionInfo());

        //if (LS->PackageName.ToString().contains("eather")) continue;
        //read bShouldBeLoaded / bShouldBeVisible / bDisableDistanceStreaming; if all set, continue;
        WriteBool(LS, "bDisableDistanceStreaming", true);
        WriteBool(LS, "bShouldBeLoaded", true);
        WriteBool(LS, "bShouldBeVisible", true);
    }
    //if (FullDetailTiles) WriteVal<int32>(L, "LevelLODIndex", -1);
}

DWORD MainThread(HMODULE Module)
{
    g_hModule = Module;
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

    DisableCullingForStreamingLevels(UWorld::GetWorld());

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

    InitUEConsole();

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

    /*
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
    */

    GameMap Map = GetLaunchMap();

    if (Map == GameMap::Unknown)
        std::cerr << "Unknown map" << std::endl;

    if (Map == GameMap::Bodie)
    {
        std::cerr << "Launching Bodie..." << std::endl;
        UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"open PUBG_Bodie_Main?listen?game=/Game/Blueprints/Core/TeamMatchMode/BP_BattleRoyaleTeamMatchGameMode_C"), nullptr);
        OnStartMap(4);
    }
    else if (Map == GameMap::Periverka)
    {
        std::cerr << "Launching Periverka..." << std::endl;
        UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"open PUBG_School_Main?listen"), nullptr);
        OnStartMap(5);
    }
    else if (Map == GameMap::Erangel)
    {
        std::cerr << "Launching Erangel..." << std::endl;
        if (!World) World = UWorld::GetWorld();
        UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"open PUBG_Forest?listen"), nullptr);
        Sleep(15000);
        hooksagain();

        DisableCullingForStreamingLevels(UWorld::GetWorld());
        zaebal(UWorld::GetWorld());

        gRequestBuildingRefresh = true;
        // NOTE: Loot spawner is strictly delayed until players board the airplane!

        Sleep(30000);

        SDK::UWorld* World = UWorld::GetWorld();

        SDK::AGameStateBase* GS = UGameplayStatics::GetGameState(World);

        UC::TArray<SDK::APlayerState*>& Players = GS->PlayerArray;

        ASTExtraPlayerController* STEPC = (ASTExtraPlayerController*)UGameplayStatics::GetPlayerController(UWorld::GetWorld(), 0);

        std::cerr << "Players: " << Players.Num() << std::endl;

        if (Players.Num() <= 100) {
            for (int i = 0; i < 100 - Players.Num(); ++i) {
                UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), L"localspawnai 1", nullptr);
            }
        }

        std::cerr << "Players: " << Players.Num() << std::endl;
    }
    else if (Map == GameMap::Miramar)
    {
        std::cerr << "Launching Miramar..." << std::endl;
        UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"open PUBG_Desert?listen"), nullptr);
        Sleep(15000);
        hooksagain();

        DisableCullingForStreamingLevels(UWorld::GetWorld());

        gRequestBuildingRefresh = true;

        Sleep(30000);

        SDK::UWorld* World = UWorld::GetWorld();

        SDK::AGameStateBase* GS = UGameplayStatics::GetGameState(World);

        UC::TArray<SDK::APlayerState*>& Players = GS->PlayerArray;

        ASTExtraPlayerController* STEPC = (ASTExtraPlayerController*)UGameplayStatics::GetPlayerController(UWorld::GetWorld(), 0);

        std::cerr << "Players: " << Players.Num() << std::endl;

        if (Players.Num() <= 100) {
            for (int i = 0; i < 100 - Players.Num(); ++i) {
                UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), L"localspawnai 1", nullptr);
            }
        }

        std::cerr << "Players: " << Players.Num() << std::endl;
    }
    else if (Map == GameMap::Sanhok)
    {
        std::cerr << "Launching Sanhok..." << std::endl;
        UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"open /Game/Maps/PUBG_Savage/PUBG_Savage_Main?listen"), nullptr);
        Sleep(15000);
        hooksagain();

        DisableCullingForStreamingLevels(UWorld::GetWorld());

        gRequestBuildingRefresh = true;

        Sleep(30000);

        SDK::UWorld* World = UWorld::GetWorld();

        SDK::AGameStateBase* GS = UGameplayStatics::GetGameState(World);

        UC::TArray<SDK::APlayerState*>& Players = GS->PlayerArray;

        ASTExtraPlayerController* STEPC = (ASTExtraPlayerController*)UGameplayStatics::GetPlayerController(UWorld::GetWorld(), 0);

        std::cerr << "Players: " << Players.Num() << std::endl;

        if (Players.Num() <= 100) {
            for (int i = 0; i < 100 - Players.Num(); ++i) {
                UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), L"localspawnai 1", nullptr);
            }
        }

        std::cerr << "Players: " << Players.Num() << std::endl;
    }
    else if (Map == GameMap::Vikendi)
    {
        std::cerr << "Launching Vikendi..." << std::endl;
        UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"open /Game/Maps/PUBG_DihorOtok/DihorOtok_Main?listen"), nullptr);
        Sleep(15000);
        hooksagain();

        DisableCullingForStreamingLevels(UWorld::GetWorld());

        gRequestBuildingRefresh = true;

        Sleep(30000);

        SDK::UWorld* World = UWorld::GetWorld();

        SDK::AGameStateBase* GS = UGameplayStatics::GetGameState(World);

        UC::TArray<SDK::APlayerState*>& Players = GS->PlayerArray;

        ASTExtraPlayerController* STEPC = (ASTExtraPlayerController*)UGameplayStatics::GetPlayerController(UWorld::GetWorld(), 0);

        std::cerr << "Players: " << Players.Num() << std::endl;

        if (Players.Num() <= 100) {
            for (int i = 0; i < 100 - Players.Num(); ++i) {
                UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), L"localspawnai 1", nullptr);
            }
        }

        std::cerr << "Players: " << Players.Num() << std::endl;
    }
    else if (Map == GameMap::Training)
    {
        bIsTrainingMode = true;
        std::cerr << "Launching Training..." << std::endl;
        UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"open shooting_range4?listen"), nullptr);
        //OnStartMap(6);
        Sleep(7500);
        hooksagain();
        //FixValue();
    }

    GWorld = UWorld::GetWorld();

    while (true)
    {
        //zaebal(UWorld::GetWorld());
        //Sleep(100);
    }

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
        CreateThread(0, 0, (LPTHREAD_START_ROUTINE)MainThread, hModule, 0, 0);
        break;
    }

    return TRUE;
}
