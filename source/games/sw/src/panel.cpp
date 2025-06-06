//-------------------------------------------------------------------------
/*
Copyright (C) 1997, 2005 - 3D Realms Entertainment

This file is part of Shadow Warrior version 1.2

Shadow Warrior is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

Original Source: 1997 - Frank Maddin and Jim Norwood
Prepared for public release: 03/28/2005 - Charlie Wiederhold, 3D Realms
*/
//-------------------------------------------------------------------------

#include "ns.h"

#undef MAIN
#include "build.h"

#include "names2.h"
#include "panel.h"
#include "lists.h"
#include "misc.h"
#include "network.h"
#include "pal.h"
#include "player.h"
#include "v_2ddrawer.h"
#include "gamehud.h"

#include "weapon.h"
#include "razemenu.h"
#include "raze_sound.h"

BEGIN_SW_NS


int InitSwordAttack(DSWPlayer* pp);
DPanelSprite* InitWeaponUziSecondaryReload(DPanelSprite*);
DPanelSprite* InitWeaponUzi2(DPanelSprite*);
int InitShotgun(DSWPlayer* pp);
int InitRail(DSWPlayer* pp);
int InitMicro(DSWPlayer* pp);
int InitRocket(DSWPlayer* pp);
int InitNuke(DSWPlayer* pp);
int InitGrenade(DSWPlayer* pp);
int InitMine(DSWPlayer* pp);
int InitFistAttack(DSWPlayer* pp);

struct PANEL_SHRAP
{
    short xoff, yoff, skip;
    int lo_jump_speed, hi_jump_speed, lo_xspeed, hi_xspeed;
    PANEL_STATE* state[2];
};

void PanelInvTestSuicide(DPanelSprite* psp);

void InsertPanelSprite(DSWPlayer* pp, DPanelSprite* psp);
void pKillSprite(DPanelSprite* psp);
void pWeaponBob(DPanelSprite* psp, short condition);
void pSuicide(DPanelSprite* psp);
void pNextState(DPanelSprite* psp);
void pStatePlusOne(DPanelSprite* psp);
void pSetState(DPanelSprite* psp, PANEL_STATE* panel_state);
void pStateControl(DPanelSprite* psp);

int DoPanelFall(DPanelSprite* psp);
int DoBeginPanelFall(DPanelSprite* psp);
int DoPanelJump(DPanelSprite* psp);
int DoBeginPanelJump(DPanelSprite* psp);
void SpawnHeartBlood(DPanelSprite* psp);
void SpawnUziShell(DPanelSprite* psp);

bool pWeaponUnHideKeys(DPanelSprite* psp, PANEL_STATE* state);
bool pWeaponHideKeys(DPanelSprite* psp, PANEL_STATE* state);
void pHotHeadOverlays(DPanelSprite* psp, short mode);

uint8_t UziRecoilYadj = 0;

extern short screenpeek;

pANIMATOR pNullAnimator;
int InitStar(DSWPlayer*);
int ChangeWeapon(DSWPlayer*);

ANIMATOR InitFire;

int NullAnimator(DSWActor*)
{
    return 0;
}

void pNullAnimator(DPanelSprite*)
{
    return;
}

inline int pspheight(DPanelSprite* psp)
{
    auto tex = tileGetTexture(psp->picndx);
    return (int)tex->GetDisplayHeight();
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

DPanelSprite* pFindMatchingSprite(DSWPlayer* pp, int x, int y, short pri)
{
    DPanelSprite* next;

    auto list = pp->GetPanelSpriteList();
    for (auto psp = list->Next; next = psp->Next, psp != list; psp = next)
    {
        // early out
        if (psp->priority > pri)
            return nullptr;

        if (psp->pos.X == x && psp->pos.Y == y && psp->priority == pri)
        {
            return psp;
        }
    }

    return nullptr;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

DPanelSprite* pFindMatchingSpriteID(DSWPlayer* pp, short id, int x, int y, short pri)
{
    DPanelSprite* next;

    auto list = pp->GetPanelSpriteList();
    for (auto psp = list->Next; next = psp->Next, psp != list; psp = next)
    {
        // early out
        if (psp->priority > pri)
            return nullptr;

        if (psp->ID == id && psp->pos.X == x && psp->pos.Y == y && psp->priority == pri)
        {
            return psp;
        }
    }

    return nullptr;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

bool pKillScreenSpiteIDs(DSWPlayer* pp, short id)
{
    DPanelSprite* next;
    bool found = false;

    // Kill ALL sprites with the correct id
    auto list = pp->GetPanelSpriteList();
    for (auto psp = list->Next; next = psp->Next, psp != list; psp = next)
    {
        if (psp->ID == id)
        {
            pKillSprite(psp);
            found = true;
        }
    }

    return found;
}


//---------------------------------------------------------------------------
//
// Used to sprites in the view at correct aspect ratio and x,y location.
//
//---------------------------------------------------------------------------

void pSetSuicide(DPanelSprite* psp)
{
    //psp->flags |= (PANF_SUICIDE);
    //psp->State = nullptr;
    psp->PanelSpriteFunc = pSuicide;
}

void pToggleCrosshair(void)
{
	cl_crosshair = !cl_crosshair;
}

//---------------------------------------------------------------------------
//
// Player has a chance of yelling out during combat, when firing a weapon.
//
//---------------------------------------------------------------------------

void DoPlayerChooseYell(DSWPlayer* pp)
{
    int choose_snd = 0;

    if (RandomRange(1000) < 990) return;

    choose_snd = StdRandomRange(MAX_YELLSOUNDS);

    if (pp == getPlayer(myconnectindex))
        PlayerSound(PlayerYellVocs[choose_snd], v3df_follow|v3df_dontpan,pp);
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void ArmorCalc(int damage_amt, int *armor_damage, int *player_damage)
{
    int damage_percent;

    if (damage_amt == 1)
    {
        *player_damage = RANDOM_P2(2<<8)>>8;
        *armor_damage = 1;
        return;
    }

    // note: this could easily be converted to a mulscale and save a
    // bit of processing for floats
    damage_percent = int((0.6 * damage_amt)+0.5);

    *player_damage = damage_amt - damage_percent;
    *armor_damage = damage_percent;
}


//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void PlayerUpdateHealth(DSWPlayer* pp, short value)
{
    DSWActor* plActor = pp->GetActor();
    short x,y;

    if (Prediction)
        return;

    if (GodMode)
    {
        if (value < 0)
            return;
    }

    if (pp->Flags & (PF_DEAD))
        return;

    if (value < 0)
    {
        bool IsChem = false;
        bool NoArmor = false;

        if (value <= -2000)
        {
            value += 2000;
            NoArmor = true;
        }
        else if (value <= -1000)
        {
            value += 1000;
            IsChem = true;
        }

        // TAKE SOME DAMAGE
        plActor->user.LastDamage = -value;

        // adjust for armor
        if (pp->Armor && !NoArmor)
        {
            int armor_damage, player_damage;
            ArmorCalc(abs(value), &armor_damage, &player_damage);
            PlayerUpdateArmor(pp, -armor_damage);
            value = -player_damage;
        }

        plActor->user.Health += value;

        if (value < 0)
        {
            int choosesnd = 0;

            choosesnd = RandomRange(MAX_PAIN);

            if (plActor->user.Health > 50)
            {
                PlayerSound(PlayerPainVocs[choosesnd],v3df_dontpan|v3df_doppler|v3df_follow,pp);
            }
            else
            {
                PlayerSound(PlayerLowHealthPainVocs[choosesnd],v3df_dontpan|v3df_doppler|v3df_follow,pp);
            }

        }
        // Do redness based on damage taken.
        if (pp == getPlayer(screenpeek))
        {
            if (IsChem)
                SetFadeAmt(pp,-40,144);  // ChemBomb green color
            else
            {
                if (value <= -10)
                    SetFadeAmt(pp,value,112);
                else if (value > -10 && value < 0)
                    SetFadeAmt(pp,-20,112);
            }
        }
        if (plActor->user.Health <= 100)
            pp->MaxHealth = 100; // Reset max health if sank below 100
    }
    else
    {
        // ADD SOME HEALTH
        if (value > 1000)
            plActor->user.Health += (value-1000);
        else
        {
            if (plActor->user.Health < pp->MaxHealth)
            {
                plActor->user.Health+=value;
            }
        }


        if (value >= 1000)
            value -= 1000;  // Strip out high value
    }

    if (plActor->user.Health < 0)
        plActor->user.Health = 0;

    if (plActor->user.Health > pp->MaxHealth)
        plActor->user.Health = pp->MaxHealth;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void PlayerUpdateAmmo(DSWPlayer* pp, short UpdateWeaponNum, short value)
{
    short x,y;
    short WeaponNum;

    if (Prediction)
        return;

    if (DamageData[UpdateWeaponNum].max_ammo == -1)
    {
        return;
    }

    WeaponNum = UpdateWeaponNum;

    // get the WeaponNum of the ammo
    if (DamageData[UpdateWeaponNum].with_weapon != -1)
    {
        WeaponNum = DamageData[UpdateWeaponNum].with_weapon;
    }

    pp->WpnAmmo[WeaponNum] += value;

    if (pp->WpnAmmo[WeaponNum] <= 0)
    {
        // star and mine
        if (WeaponIsAmmo & BIT(WeaponNum))
            pp->WpnFlags &= ~BIT(WeaponNum);

        pp->WpnAmmo[WeaponNum] = 0;
    }

    if (pp->WpnAmmo[WeaponNum] > DamageData[WeaponNum].max_ammo)
    {
        pp->WpnAmmo[WeaponNum] = DamageData[WeaponNum].max_ammo;
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void PlayerUpdateWeapon(DSWPlayer* pp, short WeaponNum)
{
    DSWActor* plActor = pp->GetActor();

    // weapon Change
    if (Prediction)
        return;

    plActor->user.WeaponNum = int8_t(WeaponNum);
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void PlayerUpdateKills(DSWPlayer* pp, short value)
{
    if (Prediction)
        return;

    if (gNet.MultiGameType == MULTI_GAME_COOPERATIVE)
        return;

    // Team play
    if (gNet.MultiGameType == MULTI_GAME_COMMBAT && gNet.TeamPlay)
    {
        short pnum;
        DSWPlayer* opp;

        TRAVERSE_CONNECT(pnum)
        {
            opp = getPlayer(pnum);

            // for everyone on the same team
            if (opp != pp && opp->GetActor()->user.spal == pp->GetActor()->user.spal)
            {
                Level.addFrags(pnum, value);
            }
        }
    }

    Level.addFrags(pp->pnum, value);
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void PlayerUpdateArmor(DSWPlayer* pp, short value)
{
    if (Prediction)
        return;

    if (value >= 1000)
        pp->Armor = value-1000;
    else
        pp->Armor += value;

    if (pp->Armor > 100)
        pp->Armor = 100;
    if (pp->Armor < 0)
        pp->Armor = 0;
}


//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

int WeaponOperate(DSWPlayer* pp)
{
    short weapon;
    DSWActor* plActor = pp->GetActor();

    InventoryKeys(pp);

    // UziType >= 3 means they are reloading
    if (pp->WpnUziType >= 3) return true;

    //if (CheatInputMode)
    //    return (0);

    if (pp->FirePause != 0)
    {
        pp->FirePause -= synctics;
        if (pp->FirePause <= 0)
            pp->FirePause = 0;
    }

    if (pp->sop)
    {
        switch (pp->sop->track)
        {
        case SO_VEHICLE:
        case SO_TURRET:
        case SO_TURRET_MGUN:
        // case SO_SPEED_BOAT:

            if (!(pp->sop->flags & SOBJ_HAS_WEAPON))
                break;

            if (pp->cmd.ucmd.actions & SB_FIRE)
            {
                if (pp->KeyPressBits & SB_FIRE)
                {
                    if (!pp->FirePause)
                    {
                        InitSobjGun(pp);
                    }
                }
            }

            return 0;
        }
    }

    weapon = pp->cmd.ucmd.getNewWeapon();

    if (weapon)
    {
        if (pp->KeyPressBits & SB_FIRST_WEAPON_BIT)
        {
            pp->KeyPressBits &= ~SB_FIRST_WEAPON_BIT;

            weapon -= 1;

            // Special uzi crap
            if (weapon != WPN_UZI) pp->WpnUziType = 2; // This means we aren't on uzi

            switch (weapon)
            {
            case WPN_FIST:
                //case WPN_SWORD:
                if (plActor->user.WeaponNum == WPN_FIST
                    || plActor->user.WeaponNum == WPN_SWORD)
                {
                    // toggle
                    if (pp->WpnFirstType == WPN_FIST)
                        pp->WpnFirstType = WPN_SWORD;
                    else if (pp->WpnFirstType == WPN_SWORD)
                        pp->WpnFirstType = WPN_FIST;
                }

                switch (pp->WpnFirstType)
                {
                case WPN_SWORD:
                    InitWeaponSword(pp);
                    break;
                case WPN_FIST:
                    InitWeaponFist(pp);
                    break;
                }
                break;
            case WPN_STAR:
                InitWeaponStar(pp);
                break;
            case WPN_UZI:
                if (plActor->user.WeaponNum == WPN_UZI)
                {
                    if (pp->Flags & (PF_TWO_UZI))
                    {
                        pp->WpnUziType++;
                        PlaySound(DIGI_UZI_UP, pp, v3df_follow);
                        if (pp->WpnUziType > 1)
                            pp->WpnUziType = 0;
                    }
                    else
                        pp->WpnUziType = 1; // Use retracted state for single uzi
                }
                InitWeaponUzi(pp);
                break;
            case WPN_MICRO:
                if (plActor->user.WeaponNum == WPN_MICRO)
                {
                    pp->WpnRocketType++;
                    PlaySound(DIGI_ROCKET_UP, pp, v3df_follow);
                    if (pp->WpnRocketType > 2)
                        pp->WpnRocketType = 0;
                    if (pp->WpnRocketType == 2 && pp->WpnRocketNuke == 0)
                        pp->WpnRocketType = 0;
                    if (pp->WpnRocketType == 2)
                        pp->TestNukeInit = true; // Init the nuke
                    else
                        pp->TestNukeInit = false;
                }
                InitWeaponMicro(pp);
                break;
            case WPN_SHOTGUN:
                if (plActor->user.WeaponNum == WPN_SHOTGUN)
                {
                    pp->WpnShotgunType++;
                    if (pp->WpnShotgunType > 1)
                        pp->WpnShotgunType = 0;
                    PlaySound(DIGI_SHOTGUN_UP, pp, v3df_follow);
                }
                InitWeaponShotgun(pp);
                break;
            case WPN_RAIL:
                if (!SW_SHAREWARE)
                {
#if 0
                    if (plActor->user.WeaponNum == WPN_RAIL)
                    {
                        pp->WpnRailType++;
                        if (pp->WpnRailType > 1)
                            pp->WpnRailType = 0;
                    }
                    if (pp->WpnRailType == 1)
                        PlaySound(DIGI_RAIL_UP, pp, v3df_follow);
#endif
                    InitWeaponRail(pp);
                }
                else
                {
                    PutStringInfo(pp,"Order the full version");
                }
                break;
            case WPN_HOTHEAD:
                if (!SW_SHAREWARE)
                {
                    if (plActor->user.WeaponNum == WPN_HOTHEAD
                        || plActor->user.WeaponNum == WPN_RING
                        || plActor->user.WeaponNum == WPN_NAPALM)
                    {
                        pp->WpnFlameType++;
                        if (pp->WpnFlameType > 2)
                            pp->WpnFlameType = 0;
//                      if(pp->Wpn[WPN_HOTHEAD])
                        pHotHeadOverlays(pp->Wpn[WPN_HOTHEAD], pp->WpnFlameType);
                        PlaySound(DIGI_HOTHEADSWITCH, pp, v3df_dontpan|v3df_follow);
                    }

                    InitWeaponHothead(pp);
                }
                else
                {
                    PutStringInfo(pp,"Order the full version");
                }
                break;
            case WPN_HEART:
                if (!SW_SHAREWARE)
                {
                    InitWeaponHeart(pp);
                }
                else
                {
                    PutStringInfo(pp,"Order the full version");
                }
                break;
            case WPN_GRENADE:
                InitWeaponGrenade(pp);
                break;
            case WPN_MINE:
                //InitChops(pp);
                InitWeaponMine(pp);
                break;
            case 13:
                pp->WpnFirstType = WPN_FIST;
                InitWeaponFist(pp);
                break;
            case 14:
                pp->WpnFirstType = WPN_SWORD;
                InitWeaponSword(pp);
                break;
            }
        }
    }
    else
    {
        pp->KeyPressBits |= SB_FIRST_WEAPON_BIT;
    }

    // Shut that computer chick up if weapon has changed!
    // This really should be handled better, but since there's no usable tracking state for the sounds, the easiest way to handle them is to play on otherwise unused channels.
    if (pp->WpnRocketType != 2 || pp->CurWpn != pp->Wpn[WPN_MICRO])
    {
        pp->InitingNuke = false;
        soundEngine->StopSound(SOURCE_Player, pp, CHAN_WEAPON);
    }
    if (pp->CurWpn != pp->Wpn[WPN_RAIL])
    {
        soundEngine->StopSound(SOURCE_Player, pp, CHAN_ITEM);
    }

    return 0;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

bool WeaponOK(DSWPlayer* pp)
{
    short min_ammo, WeaponNum, FindWeaponNum;
    static const uint8_t wpn_order[] = {2,3,4,5,6,7,8,9,1,0};
    unsigned wpn_ndx=0;

    DSWActor* plActor = pp->GetActor();

    if (!plActor || !plActor->hasU())
        return(false);

    // sword
    if (DamageData[plActor->user.WeaponNum].max_ammo == -1)
        return true;

    WeaponNum = plActor->user.WeaponNum;
    FindWeaponNum = plActor->user.WeaponNum;

    min_ammo = DamageData[WeaponNum].min_ammo;

    // if ran out of ammo switch to something else
    if (pp->WpnAmmo[WeaponNum] < min_ammo)
    {
        if (plActor->user.WeaponNum == WPN_UZI) pp->WpnUziType = 2; // Set it for retract

        // Still got a nuke, it's ok.
        if (WeaponNum == WPN_MICRO && pp->WpnRocketNuke)
        {
            //pp->WpnRocketType = 2; // Set it to Nuke
            if (!pp->NukeInitialized) pp->TestNukeInit = true;

            plActor->user.WeaponNum = WPN_MICRO;
            (*DamageData[plActor->user.WeaponNum].Init)(pp);

            return true;
        }

        pp->KeyPressBits &= ~SB_FIRE;

        FindWeaponNum = WPN_SHOTGUN; // Start at the top

        while (true)
        {
            // ran out of weapons - choose SWORD
            if (wpn_ndx > sizeof(wpn_order))
            {
                FindWeaponNum = WPN_SWORD;
                break;
            }

            // if you have the weapon and the ammo is greater than 0
            if (pp->WpnFlags & (BIT(FindWeaponNum)) && pp->WpnAmmo[FindWeaponNum] >= min_ammo)
                break;

            wpn_ndx++;
            FindWeaponNum = wpn_order[wpn_ndx];
        }

        plActor->user.WeaponNum = int8_t(FindWeaponNum);

        if (plActor->user.WeaponNum == WPN_HOTHEAD)
        {
            pp->WeaponType = WPN_HOTHEAD;
            pp->WpnFlameType = 0;
        }

        (*DamageData[plActor->user.WeaponNum].Init)(pp);

        return false;
    }

    return true;
}

//---------------------------------------------------------------------------
//
// X/Y POSITION INLINES
//
//---------------------------------------------------------------------------


inline double pspSinVel(DPanelSprite* const psp, int const ang = INT_MAX)
{
    return psp->vel * synctics * BobVal(ang == INT_MAX ? psp->ang : ang) / 256.;
}

inline double pspCosVel(DPanelSprite* const psp, int const ang = INT_MAX)
{
    return psp->vel * synctics * BobVal((ang == INT_MAX ? psp->ang : ang) + 512) / 256.;
}

inline double pspPresentRetractScale(DPanelSprite* psp, double const defaultheight)
{
    double const picheight = pspheight(psp);
    return picheight == defaultheight ? 1 : picheight / defaultheight;
}


//////////////////////////////////////////////////////////////////////////////////////////
//
// SWORD
//
//////////////////////////////////////////////////////////////////////////////////////////

static short SwordAngTable[] =
{
    82,
    168,
    256+64
};

short SwordAng = 0;

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void SwordBlur(DPanelSprite* psp)
{
    psp->kill_tics -= synctics;
    if (psp->kill_tics <= 0)
    {
        pKillSprite(psp);
        return;
    }
    else if (psp->kill_tics <= 6)
    {
        psp->flags |= (PANF_TRANS_FLIP);
    }

    psp->shade += 10;
    // change sprites priority
    REMOVE(psp);
    psp->priority--;
    InsertPanelSprite(psp->PlayerP, psp);
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void SpawnSwordBlur(DPanelSprite* psp)
{
    if (cl_nomeleeblur) return;

    DPanelSprite* nsp;
    //PICITEMp pip;

    if (psp->PlayerP->SwordAng > 200)
        return;

    nsp = pSpawnSprite(psp->PlayerP, nullptr, PRI_BACK, psp->pos.X, psp->pos.Y);

    nsp->flags |= (PANF_WEAPON_SPRITE);
    nsp->ang = psp->ang;
    nsp->vel = psp->vel;
    nsp->PanelSpriteFunc = SwordBlur;
    nsp->kill_tics = 9;
    nsp->shade = psp->shade + 10;

    nsp->picndx = -1;
    nsp->picnum = psp->picndx;

    if ((psp->State->flags & psf_Xflip))
        nsp->flags |= (PANF_XFLIP);

    nsp->rotate_ang = psp->rotate_ang;
    nsp->scale = psp->scale;

    nsp->flags |= (PANF_TRANSLUCENT);
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pSwordPresent(DPanelSprite* psp);
void pSwordRetract(DPanelSprite* psp);
void pSwordAction(DPanelSprite* psp);
void pSwordRest(DPanelSprite* psp);
void pSwordAttack(DPanelSprite* psp);
void pSwordSlide(DPanelSprite* psp);
void pSwordSlideDown(DPanelSprite* psp);
void pSwordHide(DPanelSprite* psp);
void pSwordUnHide(DPanelSprite* psp);

void pSwordSlideR(DPanelSprite* psp);
void pSwordSlideDownR(DPanelSprite* psp);

extern PANEL_STATE ps_SwordSwing[];
extern PANEL_STATE ps_ReloadSword[];

#define Sword_BEAT_RATE 24
#define Sword_ACTION_RATE 10

PANEL_STATE ps_PresentSword[] =
{
    {ID_SwordPresent0, Sword_BEAT_RATE, pSwordPresent, &ps_PresentSword[0], 0,0,0}
};

PANEL_STATE ps_SwordRest[] =
{
    {ID_SwordPresent0, Sword_BEAT_RATE, pSwordRest, &ps_SwordRest[0], 0,0,0}
};

PANEL_STATE ps_SwordHide[] =
{
    {ID_SwordPresent0, Sword_BEAT_RATE, pSwordHide, &ps_SwordHide[0], 0,0,0}
};

#define SWORD_PAUSE_TICS 10
//#define SWORD_SLIDE_TICS 14
#define SWORD_SLIDE_TICS 10
#define SWORD_MID_SLIDE_TICS 14

PANEL_STATE ps_SwordSwing[] =
{
    {ID_SwordSwing0, SWORD_PAUSE_TICS,                    pNullAnimator,      &ps_SwordSwing[1], 0,0,0},
    {ID_SwordSwing1, SWORD_SLIDE_TICS, /* start slide */ pSwordSlide,        &ps_SwordSwing[2], 0,0,0},
    {ID_SwordSwing1, 0, /* damage */ pSwordAttack,       &ps_SwordSwing[3], psf_QuickCall, 0,0},
    {ID_SwordSwing2, SWORD_MID_SLIDE_TICS, /* mid slide */ pSwordSlideDown,    &ps_SwordSwing[4], 0,0,0},

    {ID_SwordSwing2, 99, /* end slide */ pSwordSlideDown,    &ps_SwordSwing[4], 0,0,0},

    {ID_SwordSwingR1, SWORD_SLIDE_TICS, /* start slide */ pSwordSlideR,       &ps_SwordSwing[6], psf_Xflip, 0,0},
    {ID_SwordSwingR2, 0, /* damage */ pSwordAttack,       &ps_SwordSwing[7], psf_QuickCall|psf_Xflip, 0,0},
    {ID_SwordSwingR2, SWORD_MID_SLIDE_TICS, /* mid slide */ pSwordSlideDownR,   &ps_SwordSwing[8], psf_Xflip, 0,0},

    {ID_SwordSwingR2, 99, /* end slide */ pSwordSlideDownR,   &ps_SwordSwing[8], psf_Xflip, 0,0},
    {ID_SwordSwingR2, 2, /* end slide */ pNullAnimator,      &ps_SwordSwing[1], psf_Xflip, 0,0},
};

PANEL_STATE ps_RetractSword[] =
{
    {ID_SwordPresent0, Sword_BEAT_RATE, pSwordRetract, &ps_RetractSword[0], 0,0,0}
};

#define SWORD_SWAY_AMT 12

// left swing
#define SWORD_XOFF (320 + SWORD_SWAY_AMT)
#define SWORD_YOFF 200

// right swing
#define SWORDR_XOFF (0 - 80)

#define SWORD_VEL 1700
#define SWORD_POWER_VEL 2500


//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void SpecialUziRetractFunc(DPanelSprite* psp)
{
    psp->backupy();
    psp->pos.Y += 4 * synctics;

    auto tex = tileGetTexture(psp->picnum);
    if (psp->pos.Y >= 200 + (int)tex->GetDisplayHeight())
    {
        pKillSprite(psp);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void RetractCurWpn(DSWPlayer* pp)
{
    // Retract old weapon
    if (pp->CurWpn)
    {
        DPanelSprite* cur,* nxt;

        if ((pp->CurWpn == pp->Wpn[WPN_UZI] && pp->WpnUziType == 2) || pp->CurWpn != pp->Wpn[WPN_UZI])
        {
            pSetState(pp->CurWpn, pp->CurWpn->RetractState);
            pp->Flags |= (PF_WEAPON_RETRACT);
        }

        if (pp->CurWpn->sibling)
        {
            // primarily for double uzi to take down the second uzi
            pSetState(pp->CurWpn->sibling, pp->CurWpn->sibling->RetractState);
        }
        else
        {
            // check for any outstanding siblings that need to go away also
            auto list = pp->GetPanelSpriteList();
            for (cur = list->Next; nxt = cur->Next, cur != list; cur = nxt)
            {
                if (cur->sibling && cur->sibling == pp->CurWpn)
                {
                    // special case for uzi reload pieces
                    cur->picnum = cur->picndx;
                    cur->State = nullptr;
                    cur->PanelSpriteFunc = SpecialUziRetractFunc;
                    cur->sibling = nullptr;
                }
            }
        }
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void InitWeaponSword(DSWPlayer* pp)
{
    DPanelSprite* psp;
    short rnd_num;


    if (Prediction)
        return;

    if (!(pp->WpnFlags &BIT(WPN_SWORD)) ||
        (pp->Flags & PF_WEAPON_RETRACT))
        return;

    // needed for death sequence when the SWORD was your weapon when you died
    if (pp->Wpn[WPN_SWORD] && (pp->Wpn[WPN_SWORD]->flags & PANF_DEATH_HIDE))
    {
        pp->Wpn[WPN_SWORD]->flags &= ~(PANF_DEATH_HIDE);
        pp->Flags &= ~(PF_WEAPON_RETRACT|PF_WEAPON_DOWN);
        pSetState(pp->CurWpn, pp->CurWpn->PresentState);
        return;
    }


    if (!pp->Wpn[WPN_SWORD])
    {
        psp = pp->Wpn[WPN_SWORD] = pSpawnSprite(pp, ps_PresentSword, PRI_MID, SWORD_XOFF, SWORD_YOFF);
        psp->pos.Y += pspheight(psp);
        psp->backupy();
    }

    if (pp->CurWpn == pp->Wpn[WPN_SWORD])
    {
        return;
    }

    PlayerUpdateWeapon(pp, WPN_SWORD);

    pp->WpnUziType = 2; // Make uzi's go away!
    RetractCurWpn(pp);

    // Set up the new weapon variables
    psp = pp->CurWpn = pp->Wpn[WPN_SWORD];
    psp->flags |= (PANF_WEAPON_SPRITE);
    psp->ActionState = ps_SwordSwing;
    psp->RetractState = ps_RetractSword;
    psp->PresentState = ps_PresentSword;
    psp->RestState = ps_SwordRest;
    pSetState(psp, psp->PresentState);

    PlaySound(DIGI_SWORD_UP, pp, v3df_follow|v3df_dontpan);

    if (pp == getPlayer(myconnectindex) && PlayClock > 0)
    {
        rnd_num = StdRandomRange(1024);
        if (rnd_num > 900)
            PlaySound(DIGI_TAUNTAI2, pp, v3df_follow|v3df_dontpan);
        else if (rnd_num > 800)
            PlaySound(DIGI_PLAYERYELL1, pp, v3df_follow|v3df_dontpan);
        else if (rnd_num > 700)
            PlaySound(DIGI_PLAYERYELL2, pp, v3df_follow|v3df_dontpan);
        else if (rnd_num > 600)
            PlayerSound(DIGI_ILIKESWORD, v3df_follow|v3df_dontpan,pp);
    }

    psp->PlayerP->KeyPressBits |= SB_FIRE;
}


//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pSwordPresent(DPanelSprite* psp)
{
    if (psp->PlayerP->Flags & (PF_WEAPON_RETRACT))
        return;

    psp->backupy();
    psp->pos.Y -= 3 * synctics * pspPresentRetractScale(psp, 136);

    if (psp->pos.Y < SWORD_YOFF)
    {
        psp->opos.Y = psp->pos.Y = SWORD_YOFF;
        psp->backupboby();
        pSetState(psp, psp->RestState);
    }
}

//---------------------------------------------------------------------------
//
// LEFT SWING
//
//---------------------------------------------------------------------------

void pSwordSlide(DPanelSprite* psp)
{
    SpawnSwordBlur(psp);

    psp->backupcoords();

    psp->pos.X += pspCosVel(psp);
    psp->pos.Y -= pspSinVel(psp);

    psp->vel += 24 * synctics;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pSwordSlideDown(DPanelSprite* psp)
{
    SpawnSwordBlur(psp);

    auto ang = SwordAng + psp->ang + psp->PlayerP->SwordAng;

    psp->backupcoords();

    psp->pos.X += pspCosVel(psp, ang);
    psp->pos.Y -= pspSinVel(psp, ang);

    psp->vel += 20 * synctics;

    if (psp->pos.X < -40)
    {
        // if still holding down the fire key - continue swinging
        if (psp->PlayerP->cmd.ucmd.actions & SB_FIRE)
        {
            if (psp->PlayerP->KeyPressBits & SB_FIRE)
            {
                DoPlayerChooseYell(psp->PlayerP);
                // continue to next state to swing right
                pStatePlusOne(psp);
                psp->opos.X = psp->pos.X = SWORDR_XOFF;
                psp->opos.Y = psp->pos.Y = SWORD_YOFF;
                psp->backupboby();
                psp->ang = 1024;
                psp->PlayerP->SwordAng = SwordAngTable[RandomRange(SIZ(SwordAngTable))];
                psp->vel = 2500;
                DoPlayerSpriteThrow(psp->PlayerP);
                return;
            }
        }

        // NOT still holding down the fire key - stop swinging
        pSetState(psp, psp->PresentState);
        psp->opos.X = psp->pos.X = SWORD_XOFF;
        psp->opos.Y = psp->pos.Y = SWORD_YOFF + pspheight(psp);
        psp->backupboby();
    }
}

//---------------------------------------------------------------------------
//
// RIGHT SWING
//
//---------------------------------------------------------------------------

void pSwordSlideR(DPanelSprite* psp)
{
    SpawnSwordBlur(psp);

    psp->backupcoords();

    psp->pos.X -= pspCosVel(psp);
    psp->pos.Y += pspSinVel(psp);

    psp->vel += 24 * synctics;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pSwordSlideDownR(DPanelSprite* psp)
{
    SpawnSwordBlur(psp);

    auto ang = SwordAng + psp->ang - psp->PlayerP->SwordAng;

    psp->backupcoords();

    psp->pos.X -= pspCosVel(psp, ang);
    psp->pos.Y += pspSinVel(psp, ang);

    psp->vel += 24 * synctics;

    if (psp->pos.X > 350)
    {
        // if still holding down the fire key - continue swinging
        if (psp->PlayerP->cmd.ucmd.actions & SB_FIRE)
        {
            if (psp->PlayerP->KeyPressBits & SB_FIRE)
            {
                DoPlayerChooseYell(psp->PlayerP);
                // back to action state
                pStatePlusOne(psp);
                psp->opos.X = psp->pos.X = SWORD_XOFF + 80;
                psp->opos.Y = psp->pos.Y = SWORD_YOFF;
                psp->backupboby();
                psp->PlayerP->SwordAng = SwordAngTable[RandomRange(SIZ(SwordAngTable))];
                psp->ang = 1024;
                psp->vel = 2500;
                DoPlayerSpriteThrow(psp->PlayerP);
                return;
            }
        }

        // NOT still holding down the fire key - stop swinging
        pSetState(psp, psp->PresentState);
        psp->opos.X = psp->pos.X = SWORD_XOFF;
        psp->opos.Y = psp->pos.Y = SWORD_YOFF + pspheight(psp);
        psp->backupboby();
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pSwordBobSetup(DPanelSprite* psp)
{
    if (psp->flags & (PANF_BOB))
        return;

    psp->backupbobcoords();

    psp->sin_amt = SWORD_SWAY_AMT;
    psp->sin_ndx = 0;
    psp->bob_height_divider = 8;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pSwordHide(DPanelSprite* psp)
{
    psp->backupy();
    psp->pos.Y += 3 * synctics;

    if (psp->pos.Y >= SWORD_YOFF + pspheight(psp))
    {
        psp->opos.Y = psp->pos.Y = SWORD_YOFF + pspheight(psp);
        psp->opos.X = psp->pos.X = SWORD_XOFF;

        pWeaponUnHideKeys(psp, psp->PresentState);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pSwordRest(DPanelSprite* psp)
{
    bool force = !!(psp->flags & PANF_UNHIDE_SHOOT);

    if (pWeaponHideKeys(psp, ps_SwordHide))
        return;

    psp->bobpos.Y += synctics;

    if (psp->bobpos.Y > SWORD_YOFF)
    {
        psp->bobpos.Y = SWORD_YOFF;
    }

    pSwordBobSetup(psp);
    pWeaponBob(psp, PLAYER_MOVING(psp->PlayerP));

    force = !!(psp->flags & PANF_UNHIDE_SHOOT);

    if ((psp->PlayerP->cmd.ucmd.actions & SB_FIRE) || force)
    {
        if ((psp->PlayerP->KeyPressBits & SB_FIRE) || force)
        {
            psp->flags &= ~(PANF_UNHIDE_SHOOT);

            pSetState(psp, psp->ActionState);

            psp->vel = 2500;

            psp->ang = 1024;
            psp->PlayerP->SwordAng = SwordAngTable[RandomRange(SIZ(SwordAngTable))];
            DoPlayerSpriteThrow(psp->PlayerP);
        }
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pSwordAction(DPanelSprite* psp)
{
    pSwordBobSetup(psp);
    pWeaponBob(psp, PLAYER_MOVING(psp->PlayerP));
}

void pSwordAttack(DPanelSprite* psp)
{
    InitSwordAttack(psp->PlayerP);
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pSwordRetract(DPanelSprite* psp)
{
    psp->backupy();
    psp->pos.Y += 3 * synctics * pspPresentRetractScale(psp, 136);

    if (psp->pos.Y >= SWORD_YOFF + pspheight(psp))
    {
        psp->PlayerP->Flags &= ~(PF_WEAPON_RETRACT);
        psp->PlayerP->Wpn[WPN_SWORD] = nullptr;
        pKillSprite(psp);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// STAR
//
//////////////////////////////////////////////////////////////////////////////////////////

void pStarPresent(DPanelSprite* psp);
void pStarRetract(DPanelSprite* psp);
void pStarAction(DPanelSprite* psp);
void pStarRest(DPanelSprite* psp);
void pStarThrow(DPanelSprite* psp);

void pStarGrab(DPanelSprite* psp);
void pStarDone(DPanelSprite* psp);
void pStarFollowThru(DPanelSprite* psp);
void pStarFollowThru2(DPanelSprite* psp);
void pStarStartThrow(DPanelSprite* psp);

void pStarFollowUp(DPanelSprite* psp);
void pStarFollowDown(DPanelSprite* psp);

void pStarHide(DPanelSprite* psp);
void pStarRestTest(DPanelSprite* psp);

extern PANEL_STATE ps_StarThrow[];

#define PRESENT_STAR_RATE 5

PANEL_STATE ps_PresentStar[] =
{
    {ID_StarPresent0, PRESENT_STAR_RATE, pStarPresent, &ps_PresentStar[0], 0,0,0}
};

PANEL_STATE ps_StarHide[] =
{
    {ID_StarPresent0, PRESENT_STAR_RATE, pStarHide, &ps_StarHide[0], 0,0,0}
};

#define Star_RATE 2  // was 5

PANEL_STATE ps_StarRest[] =
{
    {ID_StarPresent0, Star_RATE,        pStarRest,          &ps_StarRest[0], 0,0,0},
};

PANEL_STATE ps_ThrowStar[] =
{
    {ID_StarDown0, Star_RATE+3,          pNullAnimator,      &ps_ThrowStar[1], 0,0,0},
    {ID_StarDown1, Star_RATE+3,          pNullAnimator,      &ps_ThrowStar[2], 0,0,0},
    {ID_StarDown1, Star_RATE*2,          pNullAnimator,      &ps_ThrowStar[3], psf_Invisible, 0,0},
    {ID_StarDown1, Star_RATE,            pNullAnimator,      &ps_ThrowStar[4], 0,0,0},
    {ID_StarDown0, Star_RATE,           pNullAnimator,      &ps_ThrowStar[5], 0,0,0},
    {ID_ThrowStar0, 1,                  pNullAnimator,       &ps_ThrowStar[6], 0,0,0},
    {ID_ThrowStar0, Star_RATE,          pStarThrow,         &ps_ThrowStar[7], psf_QuickCall, 0,0},
    {ID_ThrowStar0, Star_RATE,          pNullAnimator,      &ps_ThrowStar[8], 0,0,0},
    {ID_ThrowStar1, Star_RATE,          pNullAnimator,      &ps_ThrowStar[9], 0,0,0},
    {ID_ThrowStar2, Star_RATE*2,          pNullAnimator,      &ps_ThrowStar[10], 0,0,0},
    {ID_ThrowStar3, Star_RATE*2,          pNullAnimator,      &ps_ThrowStar[11], 0,0,0},
    {ID_ThrowStar4, Star_RATE*2,          pNullAnimator,      &ps_ThrowStar[12], 0,0,0},
    // start up
    {ID_StarDown1, Star_RATE+3,         pNullAnimator,         &ps_ThrowStar[13], 0,0,0},
    {ID_StarDown0, Star_RATE+3,         pNullAnimator,         &ps_ThrowStar[14], 0,0,0},
    {ID_StarPresent0, Star_RATE+3,         pNullAnimator,         &ps_ThrowStar[15], 0,0,0},
    // maybe to directly to rest state
    {ID_StarDown0, 3,                    pStarRestTest,      &ps_ThrowStar[16], psf_QuickCall, 0,0},
    // if holding the fire key we get to here
    {ID_ThrowStar4, 3,                    pNullAnimator,      &ps_ThrowStar[5], 0,0,0},
};

PANEL_STATE ps_RetractStar[] =
{
    {ID_StarPresent0, PRESENT_STAR_RATE, pStarRetract, &ps_RetractStar[0], 0,0,0}
};

//
// Right hand star routines
//

//#define STAR_YOFF 220
//#define STAR_XOFF (160+25)
#define STAR_YOFF 208
#define STAR_XOFF (160+80)

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pStarRestTest(DPanelSprite* psp)
{
    if (psp->PlayerP->cmd.ucmd.actions & SB_FIRE)
    {
        if (psp->PlayerP->KeyPressBits & SB_FIRE)
        {
            if (!WeaponOK(psp->PlayerP))
                return;

            // continue to next state to swing right
            DoPlayerChooseYell(psp->PlayerP);
            pStatePlusOne(psp);
            return;
        }
    }

    pSetState(psp, psp->RestState);
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void InitWeaponStar(DSWPlayer* pp)
{
    DPanelSprite* psp = nullptr;

    if (Prediction)
        return;

    if (!(pp->WpnFlags &BIT(WPN_STAR)) ||
        pp->WpnAmmo[WPN_STAR] < 3 ||
        (pp->Flags & PF_WEAPON_RETRACT))
    {
        //pp->WpnFirstType = WPN_SWORD;
        //InitWeaponSword(pp);
        return;
    }

    // needed for death sequence when the STAR was your weapon when you died
    if (pp->Wpn[WPN_STAR] && (pp->Wpn[WPN_STAR]->flags & PANF_DEATH_HIDE))
    {
        pp->Wpn[WPN_STAR]->flags &= ~(PANF_DEATH_HIDE);
        pp->Flags &= ~(PF_WEAPON_RETRACT);
        pSetState(pp->CurWpn, pp->CurWpn->PresentState);
        return;
    }

    if (!pp->Wpn[WPN_STAR])
    {
        psp = pp->Wpn[WPN_STAR] = pSpawnSprite(pp, ps_PresentStar, PRI_MID, STAR_XOFF, STAR_YOFF);
        psp->pos.Y += pspheight(psp);
        psp->backupy();
    }

    if (pp->CurWpn == pp->Wpn[WPN_STAR])
    {
        return;
    }

    PlayerUpdateWeapon(pp, WPN_STAR);

    pp->WpnUziType = 2; // Make uzi's go away!
    RetractCurWpn(pp);

    // Set up the new weapon variables
    pp->CurWpn = pp->Wpn[WPN_STAR];
    psp->flags |= (PANF_WEAPON_SPRITE);
    psp->ActionState = ps_ThrowStar;
    //psp->ActionState = &ps_ThrowStar[1];
    psp->RetractState = ps_RetractStar;
    psp->PresentState = ps_PresentStar;
    psp->RestState = ps_StarRest;
    //psp->RestState = ps_ThrowStar;
    pSetState(psp, psp->PresentState);

    PlaySound(DIGI_PULL, pp, v3df_follow|v3df_dontpan);
    if (StdRandomRange(1000) > 900 && pp == getPlayer(myconnectindex))
    {
        if (!sw_darts)
            PlayerSound(DIGI_ILIKESHURIKEN, v3df_follow|v3df_dontpan,pp);
    }

    psp->PlayerP->KeyPressBits |= SB_FIRE;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pStarPresent(DPanelSprite* psp)
{
    if (psp->PlayerP->Flags & (PF_WEAPON_RETRACT))
        return;

    psp->backupy();
    psp->pos.Y -= 3 * synctics;

    if (psp->pos.Y < STAR_YOFF)
    {
        psp->opos.Y = psp->pos.Y = STAR_YOFF;
    }

    if (psp->pos.Y <= STAR_YOFF)
    {
        pSetState(psp, psp->RestState);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pStarBobSetup(DPanelSprite* psp)
{
    if (psp->flags & (PANF_BOB))
        return;

    psp->backupbobcoords();

    psp->sin_amt = 10;
    psp->sin_ndx = 0;
    psp->bob_height_divider = 8;
}

void pLStarBobSetup(DPanelSprite* psp)
{
    if (psp->flags & (PANF_BOB))
        return;

    psp->backupbobcoords();

    psp->sin_amt = 6;
    psp->sin_ndx = 0;
    psp->bob_height_divider = 16;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pStarHide(DPanelSprite* psp)
{
    psp->backupy();
    psp->pos.Y += 3 * synctics;

    if (psp->pos.Y >= STAR_YOFF + pspheight(psp))
    {
        psp->opos.Y = psp->pos.Y = STAR_YOFF + pspheight(psp);

        pWeaponUnHideKeys(psp, psp->PresentState);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pStarRest(DPanelSprite* psp)
{
    bool force = !!(psp->flags & PANF_UNHIDE_SHOOT);

    if (pWeaponHideKeys(psp, ps_StarHide))
        return;

    pStarBobSetup(psp);
    pWeaponBob(psp, PLAYER_MOVING(psp->PlayerP));

    if ((psp->PlayerP->cmd.ucmd.actions & SB_FIRE) || force)
    {
        if ((psp->PlayerP->KeyPressBits & SB_FIRE) || force)
        {
            psp->flags &= ~(PANF_UNHIDE_SHOOT);

            if (!WeaponOK(psp->PlayerP))
                return;

            DoPlayerChooseYell(psp->PlayerP);

            pSetState(psp, psp->ActionState);
            DoPlayerSpriteThrow(psp->PlayerP);
        }
    }
    else
        WeaponOK(psp->PlayerP);
}


//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pStarAction(DPanelSprite* psp)
{
    pStarBobSetup(psp);
    pWeaponBob(psp, PLAYER_MOVING(psp->PlayerP));
}

void pStarThrow(DPanelSprite* psp)
{
    InitStar(psp->PlayerP);
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pStarRetract(DPanelSprite* psp)
{
    psp->backupy();
    psp->pos.Y += 3 * synctics;

    if (psp->pos.Y >= STAR_YOFF + pspheight(psp))
    {
        psp->PlayerP->Flags &= ~(PF_WEAPON_RETRACT);

        // kill only in its own routine
        psp->PlayerP->Wpn[WPN_STAR] = nullptr;
        pKillSprite(psp);
    }
}


//////////////////////////////////////////////////////////////////////////////////////////
//
// UZI
//
//////////////////////////////////////////////////////////////////////////////////////////

void pUziPresent(DPanelSprite*);
void pUziFire(DPanelSprite*);
void pUziRetract(DPanelSprite*);
void pUziAction(DPanelSprite*);
void pUziRest(DPanelSprite*);
void pUziHide(DPanelSprite*);
void pUziPresentReload(DPanelSprite*);

void pSpawnUziClip(DPanelSprite*);
void pSpawnUziReload(DPanelSprite*);
void pUziReload(DPanelSprite*);
void pUziReloadRetract(DPanelSprite*);
void pUziClip(DPanelSprite*);
void pUziDoneReload(DPanelSprite*);

void pUziEjectDown(DPanelSprite*);
void pUziEjectUp(DPanelSprite*);

// CTW MODIFICATION
//void SetVisNorm(void);
int SetVisNorm(void);
// CTW MODIFICATION END
pANIMATOR pSetVisNorm;

// Right Uzi
PANEL_STATE ps_FireUzi[] =
{
    {ID_UziPresent0, 3, pUziRest, &ps_FireUzi[0], 0,0,0},
    {ID_UziFire0, 1, pUziAction, &ps_FireUzi[2], psf_ShadeHalf, 0,0},

    {ID_UziFire1, 0, pUziFire, &ps_FireUzi[3], psf_ShadeNone|psf_QuickCall, 0,0},
    {ID_UziFire1, 4, pUziAction, &ps_FireUzi[4], psf_ShadeNone, 0,0},
    {ID_UziFire1, 0, pSetVisNorm, &ps_FireUzi[5], psf_ShadeNone|psf_QuickCall, 0,0},
    {ID_UziFire1, 4, pUziAction, &ps_FireUzi[6], psf_ShadeNone, 0,0},
    {ID_UziFire1, 0, pUziFire, &ps_FireUzi[7], psf_ShadeNone|psf_QuickCall, 0,0},
    {ID_UziFire1, 4, pUziAction, &ps_FireUzi[8], psf_ShadeNone, 0,0},
    {ID_UziFire1, 0, pSetVisNorm, &ps_FireUzi[9], psf_ShadeNone, 0,0},

    {ID_UziFire0, 4, pUziAction, &ps_FireUzi[10], psf_ShadeHalf, 0,0},
    {ID_UziFire0, 0, pUziFire, &ps_FireUzi[11], psf_QuickCall, 0,0},
    {ID_UziFire0, 4, pUziAction, &ps_FireUzi[12], psf_ShadeHalf, 0,0},
    {ID_UziFire0, 4, pUziAction, &ps_FireUzi[13], psf_ShadeHalf, 0,0},

    {ID_UziFire1, 5, pUziRest, &ps_FireUzi[0], psf_ShadeNone|psf_QuickCall, 0,0},
};

#define PRESENT_UZI_RATE 6
#define RELOAD_UZI_RATE 1

PANEL_STATE ps_UziNull[] =
{
    {ID_UziPresent0, PRESENT_UZI_RATE, pNullAnimator, &ps_UziNull[0], 0,0,0}
};

PANEL_STATE ps_UziHide[] =
{
    {ID_UziPresent0, PRESENT_UZI_RATE, pUziHide, &ps_UziHide[0], 0,0,0}
};

PANEL_STATE ps_PresentUzi[] =
{
    {ID_UziPresent0, PRESENT_UZI_RATE, pUziPresent, &ps_PresentUzi[0], 0,0,0},
};

// present of secondary uzi for reload needs to be faster
PANEL_STATE ps_PresentUziReload[] =
{
    {ID_UziPresent0, RELOAD_UZI_RATE, pUziPresentReload, &ps_PresentUziReload[0], 0,0,0},
};

PANEL_STATE ps_RetractUzi[] =
{
    {ID_UziPresent0, PRESENT_UZI_RATE, pUziRetract, &ps_RetractUzi[0], 0,0,0},
};

// Left Uzi

PANEL_STATE ps_FireUzi2[] =
{
    {ID_Uzi2Present0, 3, pUziRest, &ps_FireUzi2[0], psf_Xflip, 0,0},
    {ID_Uzi2Fire0, 1, pUziAction, &ps_FireUzi2[2], psf_ShadeHalf|psf_Xflip, 0,0},

    {ID_Uzi2Fire1, 0, pUziFire, &ps_FireUzi2[3], psf_ShadeNone|psf_QuickCall|psf_Xflip, 0,0},
    {ID_Uzi2Fire1, 4, pUziAction, &ps_FireUzi2[4], psf_ShadeNone|psf_Xflip, 0,0},
    {ID_Uzi2Fire1, 4, pUziAction, &ps_FireUzi2[5], psf_ShadeNone|psf_Xflip, 0,0},
    {ID_Uzi2Fire1, 0, pUziFire, &ps_FireUzi2[6], psf_ShadeNone|psf_QuickCall|psf_Xflip, 0,0},
    {ID_Uzi2Fire1, 4, pUziAction, &ps_FireUzi2[7], psf_ShadeNone|psf_Xflip, 0,0},

    {ID_Uzi2Fire0, 4, pUziAction, &ps_FireUzi2[8], psf_ShadeHalf|psf_Xflip, 0,0},
    {ID_Uzi2Fire0, 0, pUziFire, &ps_FireUzi2[9], psf_ShadeHalf|psf_QuickCall|psf_Xflip, 0,0},
    {ID_Uzi2Fire0, 4, pUziAction, &ps_FireUzi2[10], psf_ShadeHalf|psf_Xflip, 0,0},
    {ID_Uzi2Fire0, 4, pUziAction, &ps_FireUzi2[11], psf_ShadeHalf|psf_Xflip, 0,0},

    {ID_Uzi2Fire1, 5, pUziRest, &ps_FireUzi2[0], psf_QuickCall, 0,0},
};


PANEL_STATE ps_PresentUzi2[] =
{
    {ID_Uzi2Present0, PRESENT_UZI_RATE, pUziPresent, &ps_PresentUzi2[0], psf_Xflip, 0,0},
};

PANEL_STATE ps_Uzi2Hide[] =
{
    {ID_Uzi2Present0, PRESENT_UZI_RATE, pUziHide, &ps_Uzi2Hide[0], psf_Xflip, 0,0},
};

PANEL_STATE ps_RetractUzi2[] =
{
    {ID_Uzi2Present0, PRESENT_UZI_RATE, pUziRetract, &ps_RetractUzi2[0], psf_Xflip, 0,0},
};

PANEL_STATE ps_Uzi2Suicide[] =
{
    {ID_Uzi2Present0, PRESENT_UZI_RATE, pSuicide, &ps_Uzi2Suicide[0], psf_Xflip, 0,0}
};

PANEL_STATE ps_Uzi2Null[] =
{
    {ID_Uzi2Present0, PRESENT_UZI_RATE, pNullAnimator, &ps_Uzi2Null[0], psf_Xflip, 0,0}
};

PANEL_STATE ps_UziEject[] =
{
    {ID_UziPresent0, 1, pNullAnimator, &ps_UziEject[1], 0,0,0},
    {ID_UziPresent0, RELOAD_UZI_RATE, pUziEjectDown, &ps_UziEject[1], 0,0,0},
    {ID_UziEject0, RELOAD_UZI_RATE, pUziEjectUp, &ps_UziEject[2], 0,0,0},
    {ID_UziEject0, 1, pNullAnimator, &ps_UziEject[4], 0,0,0},
    {ID_UziEject0, RELOAD_UZI_RATE, pSpawnUziClip, &ps_UziEject[5], psf_QuickCall, 0,0},
    {ID_UziEject0, RELOAD_UZI_RATE, pNullAnimator, &ps_UziEject[5], 0,0,0},
};

PANEL_STATE ps_UziClip[] =
{
    {ID_UziClip0, RELOAD_UZI_RATE, pUziClip, &ps_UziClip[0], 0,0,0}
};

PANEL_STATE ps_UziReload[] =
{
    {ID_UziReload0, RELOAD_UZI_RATE, pUziReload, &ps_UziReload[0], 0,0,0},
    {ID_UziReload0, RELOAD_UZI_RATE, pUziReloadRetract, &ps_UziReload[1], 0,0,0}
};

PANEL_STATE ps_UziDoneReload[] =
{
    {ID_UziEject0, RELOAD_UZI_RATE, pUziDoneReload, &ps_UziDoneReload[0], 0,0,0}
};

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

#define CHAMBER_REST 0
#define CHAMBER_FIRE 1
#define CHAMBER_RELOAD 2
void pUziOverlays(DPanelSprite* psp, short mode)
{
#define UZI_CHAMBER_XOFF 32
#define UZI_CHAMBER_YOFF -73

#define UZI_CHAMBERRELOAD_XOFF 14
#define UZI_CHAMBERRELOAD_YOFF -100

    if (!(psp->flags & PANF_SECONDARY)) return;

    if (psp->over[0].xoff == -1)
    {
        psp->over[0].xoff = UZI_CHAMBER_XOFF;
        psp->over[0].yoff = UZI_CHAMBER_YOFF;
    }

    switch (mode)
    {
    case 0: // At rest
        psp->over[0].pic = UZI_COPEN;
        break;
    case 1: // Firing
        psp->over[0].pic = UZI_CLIT;
        break;
    case 2: // Reloading
        psp->over[0].pic = UZI_CRELOAD;
        psp->over[0].xoff = UZI_CHAMBERRELOAD_XOFF;
        psp->over[0].yoff = UZI_CHAMBERRELOAD_YOFF;
        break;
    }
}

#define UZI_CLIP_XOFF 16
#define UZI_CLIP_YOFF (-84)

//#define UZI_XOFF (80)
#define UZI_XOFF (100)
#define UZI_YOFF 208

#define UZI_RELOAD_YOFF 200


//---------------------------------------------------------------------------
//
// Uzi Reload
//
//---------------------------------------------------------------------------


void pUziEjectDown(DPanelSprite* gun)
{
    gun->opos.Y = gun->pos.Y;
    gun->pos.Y += 5 * synctics;

    if (gun->pos.Y > 260)
    {
        gun->opos.Y = gun->pos.Y = 260;
        pStatePlusOne(gun);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pUziEjectUp(DPanelSprite* gun)
{

    pUziOverlays(gun, CHAMBER_RELOAD);

    gun->opos.Y = gun->pos.Y;
    gun->pos.Y -= 5 * synctics;

    if (gun->pos.Y < UZI_RELOAD_YOFF)
    {
        gun->opos.Y = gun->pos.Y = UZI_RELOAD_YOFF;
        pStatePlusOne(gun);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pSpawnUziClip(DPanelSprite* gun)
{
    DPanelSprite* New;

    PlaySound(DIGI_REMOVECLIP, gun->PlayerP,v3df_follow|v3df_dontpan|v3df_doppler|v3df_follow);

    if ((gun->flags & PANF_XFLIP))
    {
        New = pSpawnSprite(gun->PlayerP, ps_UziClip, PRI_BACK, gun->pos.X - UZI_CLIP_XOFF, gun->pos.Y + UZI_CLIP_YOFF);
        New->flags |= (PANF_XFLIP);
        New->ang = NORM_ANGLE(1024 + 256 + 22);
        New->ang = NORM_ANGLE(New->ang + 512);
    }
    else
    {
        New = pSpawnSprite(gun->PlayerP, ps_UziClip, PRI_BACK, gun->pos.X + UZI_CLIP_XOFF, gun->pos.Y + UZI_CLIP_YOFF);
        New->ang = NORM_ANGLE(1024 + 256 - 22);
    }

    New->vel = 1050;
    New->flags |= (PANF_WEAPON_SPRITE);


    // carry Eject sprite with clip
    New->sibling = gun;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pSpawnUziReload(DPanelSprite* oclip)
{
    DPanelSprite* nclip;

    nclip = pSpawnSprite(oclip->PlayerP, ps_UziReload, PRI_BACK, oclip->pos.X, UZI_RELOAD_YOFF);
    nclip->flags |= PANF_WEAPON_SPRITE;

    if ((oclip->flags & PANF_XFLIP))
        nclip->flags |= PANF_XFLIP;

    // move Reload in oposite direction of clip
    nclip->ang = NORM_ANGLE(oclip->ang + 1024);
    nclip->vel = 900;

    // move gun sprite from clip to reload
    nclip->sibling = oclip->sibling;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pUziReload(DPanelSprite* nclip)
{
    DPanelSprite* gun = nclip->sibling;

    nclip->vel += 14 * synctics;

    nclip->opos.X = nclip->pos.X;
    nclip->opos.Y = nclip->pos.Y;

    nclip->pos.X += pspCosVel(nclip);
    nclip->pos.Y -= pspSinVel(nclip);

    gun->opos.X = gun->pos.X;
    gun->opos.Y = gun->pos.Y;

    gun->pos.X -= pspCosVel(gun);
    gun->pos.Y += pspSinVel(gun);

    if ((nclip->flags & PANF_XFLIP))
    {
        if (nclip->pos.X < gun->pos.X)
        {
            PlaySound(DIGI_REPLACECLIP, nclip->PlayerP,v3df_follow|v3df_dontpan|v3df_doppler);

            nclip->opos.X = nclip->pos.X = gun->pos.X - UZI_CLIP_XOFF;
            nclip->opos.Y = nclip->pos.Y = gun->pos.Y + UZI_CLIP_YOFF;
            nclip->vel = 680;
            nclip->ang = NORM_ANGLE(nclip->ang - 128 - 64);
            // go to retract phase
            pSetState(nclip, &ps_UziReload[1]);
        }
    }
    else
    {
        if (nclip->pos.X > gun->pos.X)
        {
            PlaySound(DIGI_REPLACECLIP, nclip->PlayerP,v3df_follow|v3df_dontpan|v3df_doppler);

            nclip->opos.X = nclip->pos.X = gun->pos.X + UZI_CLIP_XOFF;
            nclip->opos.Y = nclip->pos.Y = gun->pos.Y + UZI_CLIP_YOFF;
            nclip->vel = 680;
            nclip->ang = NORM_ANGLE(nclip->ang + 128 + 64);
            // go to retract phase
            pSetState(nclip, &ps_UziReload[1]);
        }
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pUziReloadRetract(DPanelSprite* nclip)
{
    DPanelSprite* gun = nclip->sibling;

    double xadj = pspCosVel(nclip);
    double yadj = pspSinVel(nclip);

    nclip->vel += 18 * synctics;

    nclip->backupcoords();

    nclip->pos.X -= xadj;
    nclip->pos.Y += yadj;

    gun->backupcoords();

    gun->pos.X -= xadj;
    gun->pos.Y += yadj;

    if (gun->pos.Y > UZI_RELOAD_YOFF + pspheight(gun))
    {
        pSetState(gun, ps_UziDoneReload);
        pKillSprite(nclip);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pUziDoneReload(DPanelSprite* psp)
{
    DSWPlayer* pp = psp->PlayerP;


    if (psp->flags & (PANF_PRIMARY) && pp->WpnUziType == 3)
    {
        // if 2 uzi's and the first one has been reloaded
        // kill the first one and make the second one the CurWeapon
        // Set uzi's back to previous state
        DPanelSprite* New;


        if (pp->WpnUziType > 2)
            pp->WpnUziType -= 3;

        New = InitWeaponUziSecondaryReload(psp);
        pp->Wpn[WPN_UZI] = New;
        pp->CurWpn = New;
        pp->CurWpn->sibling = nullptr;

        pKillSprite(psp);
        return;
    }
    else
    {
        // Reset everything

        // Set uzi's back to previous state
        if (pp->WpnUziType > 2)
            pp->WpnUziType -= 3;

        // reset uzi variable
        pp->Wpn[WPN_UZI] = nullptr;
        pp->CurWpn = nullptr;

        // kill uzi eject sequence for good
        pKillSprite(psp);

        // give the uzi back
        InitWeaponUzi(pp);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pUziClip(DPanelSprite* oclip)
{

    oclip->opos.X = oclip->pos.X;
    oclip->opos.Y = oclip->pos.Y;

    oclip->pos.X += pspCosVel(oclip);
    oclip->pos.Y -= pspSinVel(oclip);

    if (oclip->pos.Y > UZI_RELOAD_YOFF)
    {
        DPanelSprite* gun = oclip->sibling;

        // as synctics gets bigger, oclip->x can be way off
        // when clip goes off the screen - recalc oclip->x from scratch
        // so it will end up the same for all synctic values
        for (oclip->pos.X = oclip->opos.X, oclip->pos.Y = oclip->opos.Y; oclip->pos.Y < UZI_RELOAD_YOFF; )
        {
            oclip->pos.X += oclip->vel * BobVal(oclip->ang + 512) / 256.;
            oclip->pos.Y -= oclip->vel * BobVal(oclip->ang) / 256.;
        }

        oclip->opos.X = oclip->pos.X;
        oclip->opos.Y = oclip->pos.Y = UZI_RELOAD_YOFF;

        gun->vel = 800;
        gun->ang = NORM_ANGLE(oclip->ang + 1024);

        pSpawnUziReload(oclip);
        pKillSprite(oclip);
    }
}

//---------------------------------------------------------------------------
//
// Uzi Basic Stuff
//
//---------------------------------------------------------------------------

void InitWeaponUzi(DSWPlayer* pp)
{
    DPanelSprite* psp = nullptr;

    if (Prediction)
        return;

    pp->WeaponType = WPN_UZI;

    // make sure you have the uzi, uzi ammo, and not retracting another
    // weapon
    if (!(pp->WpnFlags &BIT(WPN_UZI)) ||
//        pp->WpnAmmo[WPN_UZI] <= 0 ||
        (pp->Flags & PF_WEAPON_RETRACT))
        return;

    // if players uzi is null
    if (!pp->Wpn[WPN_UZI])
    {
        psp = pp->Wpn[WPN_UZI] = pSpawnSprite(pp, ps_PresentUzi, PRI_MID, 160 + UZI_XOFF, UZI_YOFF);
        psp->pos.Y += pspheight(psp);
        psp->backupy();
    }

    // if Current weapon is uzi
    if (pp->CurWpn == pp->Wpn[WPN_UZI])
    {
        // Retracting other uzi?
        if (pp->CurWpn->sibling && pp->WpnUziType == 1)
        {
            RetractCurWpn(pp);
        }
        else
        // Is player toggling between one and two uzi's?
        if (pp->CurWpn->sibling && (pp->Wpn[WPN_UZI]->flags & PANF_PRIMARY) && pp->WpnUziType == 0)
        {
            if (!(pp->CurWpn->flags & PANF_RELOAD))
                InitWeaponUzi2(pp->Wpn[WPN_UZI]);
        }

        // if actually picked an uzi up and don't currently have double uzi
        if (pp->Flags & (PF_PICKED_UP_AN_UZI) && !(pp->Wpn[WPN_UZI]->flags & PANF_PRIMARY))
        {
            pp->Flags &= ~(PF_PICKED_UP_AN_UZI);

            if (!(pp->CurWpn->flags & PANF_RELOAD))
                InitWeaponUzi2(pp->Wpn[WPN_UZI]);
        }
        return;
    }
    else
    {
        pp->Flags &= ~(PF_PICKED_UP_AN_UZI);
    }

    PlayerUpdateWeapon(pp, WPN_UZI);

    RetractCurWpn(pp);

    // Set up the new weapon variables
    pp->CurWpn = pp->Wpn[WPN_UZI];
    psp->flags |= (PANF_WEAPON_SPRITE);
    psp->ActionState = &ps_FireUzi[1];
    psp->RetractState = ps_RetractUzi;
    psp->PresentState = ps_PresentUzi;
    psp->RestState = ps_FireUzi;
    pSetState(psp, psp->PresentState);

    // power up
    // NOTE: PRIMARY is ONLY set when there is a powerup
    if (pp->Flags & (PF_TWO_UZI))
    {
        InitWeaponUzi2(psp);
    }

    PlaySound(DIGI_UZI_UP, pp, v3df_follow);

    psp->PlayerP->KeyPressBits |= SB_FIRE;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

DPanelSprite* InitWeaponUzi2(DPanelSprite* uzi_orig)
{
    DPanelSprite* New;
    DSWPlayer* pp = uzi_orig->PlayerP;


    // There is already a second uzi, or it's retracting
    if (pp->WpnUziType == 1 || pp->CurWpn->sibling || (pp->Flags & PF_WEAPON_RETRACT)) return nullptr;

    // NOTE: PRIMARY is ONLY set when there is a powerup
    uzi_orig->flags |= PANF_PRIMARY;

    // Spawning a 2nd uzi, set weapon mode
    pp->WpnUziType = 0; // 0 is up, 1 is retract

    New = pSpawnSprite(pp, ps_PresentUzi2, PRI_MID, 160 - UZI_XOFF, UZI_YOFF);
    New->pos.Y += pspheight(New);
    New->opos.Y = New->pos.Y;
    uzi_orig->sibling = New;

    // Set up the New weapon variables
    New->flags |= (PANF_WEAPON_SPRITE);
    New->ActionState = &ps_FireUzi2[1];
    New->RetractState = ps_RetractUzi2;
    New->PresentState = ps_PresentUzi2;
    New->RestState = ps_FireUzi2;
    pSetState(New, New->PresentState);

    New->sibling = uzi_orig;
    New->flags |= (PANF_SECONDARY);
    pUziOverlays(New, CHAMBER_REST);

    return New;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

DPanelSprite* InitWeaponUziSecondaryReload(DPanelSprite* uzi_orig)
{
    DPanelSprite* New;
    DSWPlayer* pp = uzi_orig->PlayerP;

    New = pSpawnSprite(pp, ps_PresentUzi, PRI_MID, 160 - UZI_XOFF, UZI_YOFF);
    New->pos.Y += pspheight(New);
    New->opos.Y = New->pos.Y;

    New->flags |= (PANF_XFLIP);

    // Set up the New weapon variables
    New->flags |= (PANF_WEAPON_SPRITE);
    New->ActionState = ps_UziEject;
    New->RetractState = ps_RetractUzi;
    New->PresentState = ps_PresentUzi;
    New->RestState = ps_UziEject;
    // pSetState(New, New->PresentState);
    pSetState(New, ps_PresentUziReload);

    New->sibling = uzi_orig;
    New->flags |= (PANF_SECONDARY|PANF_RELOAD);

    return New;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pUziPresent(DPanelSprite* psp)
{
    if (psp->PlayerP->Flags & (PF_WEAPON_RETRACT))
        return;

    psp->backupy();
    psp->pos.Y -= 3 * synctics;

    if (psp->PlayerP->WpnUziType)
    {
        psp->PlayerP->WpnReloadState = 2;
    }

    if (psp->pos.Y < UZI_YOFF)
    {
        psp->flags &= ~(PANF_RELOAD);

        psp->opos.Y = psp->pos.Y = UZI_YOFF;
        psp->backupx();
        psp->backupbobcoords();
        pSetState(psp, psp->RestState);
    }
}

//---------------------------------------------------------------------------
//
// same as pUziPresent only faster for reload sequence
//
//---------------------------------------------------------------------------

void pUziPresentReload(DPanelSprite* psp)
{
    if (psp->PlayerP->Flags & (PF_WEAPON_RETRACT))
        return;

    psp->backupy();
    psp->pos.Y -= 5 * synctics;

    psp->PlayerP->WpnReloadState = 2;

    if (psp->pos.Y < UZI_YOFF)
    {
        psp->opos.Y = psp->pos.Y = UZI_YOFF;
        psp->backupx();
        psp->backupbobcoords();
        pSetState(psp, psp->RestState);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pUziBobSetup(DPanelSprite* psp)
{
    if (psp->flags & (PANF_BOB))
        return;

    psp->backupbobcoords();

    psp->sin_amt = 12;
    psp->sin_ndx = 0;
    psp->bob_height_divider = 8;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pUziStartReload(DPanelSprite* psp)
{
    SetVisNorm();

    psp->backupx();

    // Set uzi's to reload state
    if (psp->PlayerP->WpnUziType < 3)
        psp->PlayerP->WpnUziType += 3;

    // Uzi #1 reload - starting from a full up position
    pSetState(psp, ps_UziEject);

    psp->flags |= (PANF_RELOAD);

    if (psp->flags & (PANF_PRIMARY) && psp->sibling)
    {
        // this is going to KILL Uzi #2 !!!
        pSetState(psp->sibling, psp->sibling->RetractState);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pUziHide(DPanelSprite* psp)
{
    psp->backupy();
    psp->pos.Y += 3 * synctics;

    if (psp->pos.Y >= 200 + pspheight(psp))
    {
        psp->opos.Y = psp->pos.Y = 200 + pspheight(psp);

        if (psp->flags & (PANF_PRIMARY) && psp->PlayerP->WpnUziType != 1)
        {
            if (pWeaponUnHideKeys(psp, psp->PresentState))
                pSetState(psp->sibling, psp->sibling->PresentState);
        }
        else if (!(psp->flags & PANF_SECONDARY))
        {
            pWeaponUnHideKeys(psp, psp->PresentState);
        }
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pUziRest(DPanelSprite* psp)
{
    bool shooting;
    bool force = !!(psp->flags & PANF_UNHIDE_SHOOT);


    // If you have two uzi's, but one didn't come up, spawn it
    if (psp->PlayerP->Flags & (PF_TWO_UZI) && psp->sibling == nullptr)
    {
        InitWeaponUzi2(psp);
    }

    if (psp->flags & (PANF_PRIMARY) && psp->sibling)
    {
        if (pWeaponHideKeys(psp, ps_UziHide))
        {
            if (psp->sibling != nullptr) // !JIM! Without this line, will ASSERT if reloading here
                pSetState(psp->sibling, ps_Uzi2Hide);
            return;
        }
    }
    else if (!(psp->flags & PANF_SECONDARY))
    {
        if (pWeaponHideKeys(psp, ps_UziHide))
            return;
    }

    if (psp->flags & (PANF_SECONDARY))
        pUziOverlays(psp, CHAMBER_REST);

    SetVisNorm();

    shooting = (psp->PlayerP->cmd.ucmd.actions & SB_FIRE) && (psp->PlayerP->KeyPressBits & SB_FIRE);
    shooting |= force;

    pUziBobSetup(psp);
    pWeaponBob(psp, PLAYER_MOVING(psp->PlayerP) || shooting);

    if (shooting)
    {
        if (!WeaponOK(psp->PlayerP))
            return;

        psp->flags &= ~(PANF_UNHIDE_SHOOT);

        pSetState(psp, psp->ActionState);
    }
    else
        WeaponOK(psp->PlayerP);
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pUziAction(DPanelSprite* psp)
{
    static int alternate = 0;

    bool shooting = (psp->PlayerP->cmd.ucmd.actions & SB_FIRE) && (psp->PlayerP->KeyPressBits & SB_FIRE);

    if (shooting)
    {
        if (psp->flags & (PANF_SECONDARY))
        {
            alternate++;
            if (alternate > 6) alternate = 0;
            if (alternate <= 3)
                pUziOverlays(psp, CHAMBER_FIRE);
            else
                pUziOverlays(psp, CHAMBER_REST);
        }
        // Only Recoil if shooting
        pUziBobSetup(psp);
        UziRecoilYadj = (RANDOM_P2(1024)) >> 8;        // global hack for
        // weapon Bob
        pWeaponBob(psp, PLAYER_MOVING(psp->PlayerP) || shooting);
        UziRecoilYadj = 0;              // reset my global hack
        if (RANDOM_P2(1024) > 990)
            DoPlayerChooseYell(psp->PlayerP);
    }
    else
    {
        if (psp->flags & (PANF_SECONDARY))
            pUziOverlays(psp, CHAMBER_REST);
        pUziBobSetup(psp);
        pWeaponBob(psp, PLAYER_MOVING(psp->PlayerP) || shooting);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pUziFire(DPanelSprite* psp)
{
    DSWPlayer* pp = psp->PlayerP;

    if (!WeaponOK(psp->PlayerP))
        return;

    if (psp->flags & (PANF_SECONDARY) && pp->WpnUziType > 0) return;

    InitUzi(psp->PlayerP);
    SpawnUziShell(psp);

    // If its the second Uzi, give the shell back only if it's a reload count to keep #'s even
    if (psp->flags & (PANF_SECONDARY))
    {
        if (pp->Flags & (PF_TWO_UZI) && psp->sibling)
        {
            if ((pp->WpnAmmo[WPN_UZI] % 100) == 0)
                pp->WpnAmmo[WPN_UZI]++;
        }
        else if ((pp->WpnAmmo[WPN_UZI] % 50) == 0)
            pp->WpnAmmo[WPN_UZI]++;
    }
    else
    {
        SpawnVis(psp->PlayerP->GetActor(), nullptr, {}, 32);

        if (!WeaponOK(psp->PlayerP))
            return;

        // Reload if done with clip
        if (pp->Flags & (PF_TWO_UZI) && psp->sibling)
        {
            if ((pp->WpnAmmo[WPN_UZI] % 100) == 0)
                pUziStartReload(psp);
        }
        else if ((pp->WpnAmmo[WPN_UZI] % 50) == 0)
        {
            // clip has run out
            pUziStartReload(psp);
        }
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pUziRetract(DPanelSprite* psp)
{
    // DPanelSprite* sib = psp->sibling;
    psp->backupy();
    psp->pos.Y += 3 * synctics;

    if (psp->pos.Y >= 200 + pspheight(psp))
    {
        // if in the reload phase and its retracting then get rid of uzi
        // no matter whether it is PRIMARY/SECONDARY/neither.
        if (psp->flags & (PANF_RELOAD))
        {
            psp->PlayerP->Flags &= ~(PF_WEAPON_RETRACT);
            psp->PlayerP->Wpn[WPN_UZI] = nullptr;
        }
        else
        {
            // NOT reloading here
            if (psp->flags & (PANF_PRIMARY))
            {
                // only reset when primary goes off the screen
                psp->PlayerP->Flags &= ~(PF_WEAPON_RETRACT);
                psp->PlayerP->Wpn[WPN_UZI] = nullptr;
            }
            else if (psp->flags & (PANF_SECONDARY))
            {
                // primarily for beginning of reload sequence where seconary
                // is taken off of the screen.  Lets the primary know that
                // he is alone.
                if (psp->sibling && psp->sibling->sibling == psp)
                    psp->sibling->sibling = nullptr;
            }
            else
            {
                // only one uzi here is retracting
                psp->PlayerP->Flags &= ~(PF_WEAPON_RETRACT);
                psp->PlayerP->Wpn[WPN_UZI] = nullptr;
            }
        }


        pKillSprite(psp);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// UZI SHELL
//
//////////////////////////////////////////////////////////////////////////////////////////

void pUziShell(DPanelSprite*);

#define UZI_SHELL_RATE 10

PANEL_STATE ps_UziShell[] =
{
    {ID_UziShell0, UZI_SHELL_RATE, pUziShell, &ps_UziShell[1], 0,0,0},
    {ID_UziShell1, UZI_SHELL_RATE, pUziShell, &ps_UziShell[2], 0,0,0},
    {ID_UziShell2, UZI_SHELL_RATE, pUziShell, &ps_UziShell[3], 0,0,0},
    {ID_UziShell3, UZI_SHELL_RATE, pUziShell, &ps_UziShell[4], 0,0,0},
    {ID_UziShell4, UZI_SHELL_RATE, pUziShell, &ps_UziShell[5], 0,0,0},
    {ID_UziShell5, UZI_SHELL_RATE, pUziShell, &ps_UziShell[0], 0,0,0},
};

PANEL_STATE ps_Uzi2Shell[] =
{
    {ID_Uzi2Shell0, UZI_SHELL_RATE, pUziShell, &ps_Uzi2Shell[1], psf_Xflip, 0,0},
    {ID_Uzi2Shell1, UZI_SHELL_RATE, pUziShell, &ps_Uzi2Shell[2], psf_Xflip, 0,0},
    {ID_Uzi2Shell2, UZI_SHELL_RATE, pUziShell, &ps_Uzi2Shell[3], psf_Xflip, 0,0},
    {ID_Uzi2Shell3, UZI_SHELL_RATE, pUziShell, &ps_Uzi2Shell[4], psf_Xflip, 0,0},
    {ID_Uzi2Shell4, UZI_SHELL_RATE, pUziShell, &ps_Uzi2Shell[5], psf_Xflip, 0,0},
    {ID_Uzi2Shell5, UZI_SHELL_RATE, pUziShell, &ps_Uzi2Shell[0], psf_Xflip, 0,0},
};

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void SpawnUziShell(DPanelSprite* psp)
{
    DSWPlayer* pp = psp->PlayerP;

    if (psp->State && (psp->State->flags & psf_Xflip))
    {
        // LEFT side
        pp->UziShellLeftAlt = !pp->UziShellLeftAlt;
        if (pp->UziShellLeftAlt)
            SpawnShell(pp->GetActor(),-3);
    }
    else
    {
        // RIGHT side
        pp->UziShellRightAlt = !pp->UziShellRightAlt;
        if (pp->UziShellRightAlt)
            SpawnShell(pp->GetActor(),-2);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pUziShell(DPanelSprite* psp)
{
    psp->backupx();

    if (psp->State && (psp->State->flags & psf_Xflip))
    {
        psp->pos.X -= 3 * synctics;
    }
    else
    {
        psp->pos.X += 3 * synctics;
    }

    // increment the ndx into the sin table and wrap at 1024
    psp->sin_ndx = (psp->sin_ndx + (synctics << psp->sin_arc_speed) + 1024) & 1023;

    // get height
    psp->opos.Y = psp->pos.Y = psp->bobpos.Y;
    psp->pos.Y += psp->sin_amt * -BobVal(psp->sin_ndx);

    // if off of the screen kill them
    if (psp->pos.X > 330 || psp->pos.X < -10)
    {
        pKillSprite(psp);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// SHOTGUN SHELL
//
//////////////////////////////////////////////////////////////////////////////////////////

void pShotgunShell(DPanelSprite*);

#define SHOTGUN_SHELL_RATE 7

PANEL_STATE ps_ShotgunShell[] =
{
    {ID_ShotgunShell0, SHOTGUN_SHELL_RATE, pShotgunShell, &ps_ShotgunShell[1], 0,0,0},
    {ID_ShotgunShell1, SHOTGUN_SHELL_RATE, pShotgunShell, &ps_ShotgunShell[2], 0,0,0},
    {ID_ShotgunShell2, SHOTGUN_SHELL_RATE, pShotgunShell, &ps_ShotgunShell[3], 0,0,0},
    {ID_ShotgunShell3, SHOTGUN_SHELL_RATE, pShotgunShell, &ps_ShotgunShell[4], 0,0,0},
    {ID_ShotgunShell4, SHOTGUN_SHELL_RATE, pShotgunShell, &ps_ShotgunShell[5], 0,0,0},
    {ID_ShotgunShell5, SHOTGUN_SHELL_RATE, pShotgunShell, &ps_ShotgunShell[6], 0,0,0},
    {ID_ShotgunShell6, SHOTGUN_SHELL_RATE, pShotgunShell, &ps_ShotgunShell[7], 0,0,0},
    {ID_ShotgunShell6, SHOTGUN_SHELL_RATE, pShotgunShell, &ps_ShotgunShell[0], 0,0,0},
};

void SpawnShotgunShell(DPanelSprite* psp)
{
    DSWPlayer* pp = psp->PlayerP;
    SpawnShell(pp->GetActor(),-4);
}

void pShotgunShell(DPanelSprite* psp)
{
    if (psp->flags & (PANF_JUMPING))
    {
        DoPanelJump(psp);
    }
    else if (psp->flags & (PANF_FALLING))
    {
        DoPanelFall(psp);
    }

    psp->backupx();

    psp->pos.X += psp->xspeed * (1. / FRACUNIT);

    if (psp->pos.X > 320 || psp->pos.X < 0 || psp->pos.Y > 200)
    {
        pKillSprite(psp);
        return;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// SHOTGUN
//
//////////////////////////////////////////////////////////////////////////////////////////

void pShotgunPresent(DPanelSprite* psp);
void pShotgunRetract(DPanelSprite* psp);
void pShotgunAction(DPanelSprite* psp);
void pShotgunRest(DPanelSprite* psp);
void pShotgunFire(DPanelSprite* psp);
void pShotgunHide(DPanelSprite* psp);
void pShotgunRestTest(DPanelSprite* psp);

void pShotgunReloadDown(DPanelSprite* psp);
void pShotgunReloadUp(DPanelSprite* psp);
void pShotgunBobSetup(DPanelSprite* psp);
void pShotgunRecoilUp(DPanelSprite* psp);
void pShotgunRecoilDown(DPanelSprite* psp);

bool pShotgunReloadTest(DPanelSprite* psp);

extern PANEL_STATE ps_ShotgunReload[];

#define Shotgun_BEAT_RATE 24
#define Shotgun_ACTION_RATE 4

PANEL_STATE ps_PresentShotgun[] =
{
    {ID_ShotgunPresent0, Shotgun_BEAT_RATE, pShotgunPresent, &ps_PresentShotgun[0], 0,0,0}
};

PANEL_STATE ps_ShotgunRest[] =
{
    {ID_ShotgunPresent0, Shotgun_BEAT_RATE, pShotgunRest, &ps_ShotgunRest[0], 0,0,0}
};

PANEL_STATE ps_ShotgunHide[] =
{
    {ID_ShotgunPresent0, Shotgun_BEAT_RATE, pShotgunHide, &ps_ShotgunHide[0], 0,0,0}
};

PANEL_STATE ps_ShotgunRecoil[] =
{
    // recoil
    {ID_ShotgunReload0, Shotgun_ACTION_RATE, pShotgunRecoilDown, &ps_ShotgunRecoil[0], 0,0,0},
    {ID_ShotgunReload0, Shotgun_ACTION_RATE, pShotgunRecoilUp, &ps_ShotgunRecoil[1], 0,0,0},
    // reload
    {ID_ShotgunReload0, Shotgun_ACTION_RATE*5,  pNullAnimator,      &ps_ShotgunRecoil[3], 0,0,0},
    {ID_ShotgunReload1, Shotgun_ACTION_RATE,    pNullAnimator,      &ps_ShotgunRecoil[4], 0,0,0},
    {ID_ShotgunReload2, Shotgun_ACTION_RATE*5,  pNullAnimator,      &ps_ShotgunRecoil[5], 0,0,0},
    {ID_ShotgunPresent0,Shotgun_ACTION_RATE,    pShotgunRestTest,   &ps_ShotgunRecoil[6], 0,0,0},
    {ID_ShotgunPresent0,Shotgun_ACTION_RATE/2,  pShotgunAction,     &ps_ShotgunRecoil[7], 0,0,0},
    {ID_ShotgunPresent0,Shotgun_ACTION_RATE/2,  pShotgunAction,     &ps_ShotgunRecoil[8], 0,0,0},
    // ready to fire again
    {ID_ShotgunPresent0, 3, pNullAnimator, &ps_ShotgunRest[0], 0,0,0}
};

PANEL_STATE ps_ShotgunRecoilAuto[] =
{
    // recoil
    {ID_ShotgunReload0, 1,    pShotgunRecoilDown, &ps_ShotgunRecoilAuto[0], 0,0,0},
    {ID_ShotgunReload0, 1,    pShotgunRecoilUp,   &ps_ShotgunRecoilAuto[1], 0,0,0},
    // Reload
    {ID_ShotgunReload0, 1,    pNullAnimator,      &ps_ShotgunRecoilAuto[3], 0,0,0},
    {ID_ShotgunReload0, 1,    pNullAnimator,      &ps_ShotgunRecoilAuto[4], 0,0,0},
    {ID_ShotgunReload0, 1,    pNullAnimator,      &ps_ShotgunRecoilAuto[5], 0,0,0},

    {ID_ShotgunPresent0,1,    pShotgunRestTest,   &ps_ShotgunRecoilAuto[6], 0,0,0},
    {ID_ShotgunPresent0,1,    pShotgunAction,     &ps_ShotgunRecoilAuto[7], 0,0,0},
    {ID_ShotgunPresent0,1,    pShotgunRest,       &ps_ShotgunRest[0],psf_QuickCall, 0,0},
};

PANEL_STATE ps_ShotgunFire[] =
{
    {ID_ShotgunFire0,   Shotgun_ACTION_RATE,    pShotgunAction,     &ps_ShotgunFire[1], psf_ShadeHalf, 0,0},
    {ID_ShotgunFire1,   Shotgun_ACTION_RATE,    pShotgunFire,       &ps_ShotgunFire[2], psf_ShadeNone|psf_QuickCall, 0,0},
    {ID_ShotgunFire1,   Shotgun_ACTION_RATE,    pShotgunAction,     &ps_ShotgunFire[3], psf_ShadeNone, 0,0},
    {ID_ShotgunReload0, 0,                      SpawnShotgunShell,  &ps_ShotgunFire[4], psf_QuickCall, 0,0},
    {ID_ShotgunReload0, Shotgun_ACTION_RATE,    pShotgunAction,     &ps_ShotgunRecoil[0], 0,0,0}
};


#if 1
PANEL_STATE ps_ShotgunAutoFire[] =
{
    {ID_ShotgunFire1,   2,    pShotgunAction,     &ps_ShotgunAutoFire[1], psf_ShadeHalf, 0,0},
    {ID_ShotgunFire1,   2,    pShotgunFire,       &ps_ShotgunAutoFire[2], psf_ShadeNone, 0,0},
    {ID_ShotgunFire1,   2,    pShotgunAction,     &ps_ShotgunAutoFire[3], psf_ShadeNone, 0,0},
    {ID_ShotgunReload0, 0,    SpawnShotgunShell,  &ps_ShotgunAutoFire[4], psf_QuickCall, 0,0},
    {ID_ShotgunReload0, 1,    pShotgunAction,     &ps_ShotgunRecoilAuto[0], 0,0,0}
};
#endif

#if 1
PANEL_STATE ps_ShotgunReload[] =
{
    {ID_ShotgunPresent0, Shotgun_BEAT_RATE, pShotgunReloadDown, &ps_ShotgunReload[0], 0,0,0},
    {ID_ShotgunPresent0, 30,            pNullAnimator, &ps_ShotgunReload[2], 0,0,0},
    // make reload sound here
    {ID_ShotgunPresent0, Shotgun_BEAT_RATE, pNullAnimator, &ps_ShotgunReload[3], psf_QuickCall, 0,0},
    {ID_ShotgunPresent0, 30,            pNullAnimator, &ps_ShotgunReload[4], 0,0,0},
    {ID_ShotgunPresent0, Shotgun_BEAT_RATE, pShotgunReloadUp, &ps_ShotgunReload[4], 0,0,0},
    {ID_ShotgunPresent0, 3, pNullAnimator, &ps_ShotgunRest[0], 0,0,0}
};
#endif

PANEL_STATE ps_RetractShotgun[] =
{
    {ID_ShotgunPresent0, Shotgun_BEAT_RATE, pShotgunRetract, &ps_RetractShotgun[0], 0,0,0}
};

#define SHOTGUN_YOFF 200
#define SHOTGUN_XOFF (160+42)

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void InitWeaponShotgun(DSWPlayer* pp)
{
    DPanelSprite* psp = nullptr;

    if (Prediction)
        return;

    pp->WeaponType = WPN_SHOTGUN;

    if (!(pp->WpnFlags &BIT(pp->WeaponType)) ||
//        pp->WpnAmmo[pp->WeaponType] <= 0 ||
        (pp->Flags & PF_WEAPON_RETRACT))
        return;

    if (!pp->Wpn[pp->WeaponType])
    {
        psp = pp->Wpn[pp->WeaponType] = pSpawnSprite(pp, ps_PresentShotgun, PRI_MID, SHOTGUN_XOFF, SHOTGUN_YOFF);
        psp->pos.Y += pspheight(psp);
        psp->backupy();
    }

    if (pp->CurWpn == pp->Wpn[pp->WeaponType])
    {
        return;
    }

    psp->WeaponType = pp->WeaponType;
    PlayerUpdateWeapon(pp, pp->WeaponType);

    pp->WpnUziType = 2; // Make uzi's go away!
    RetractCurWpn(pp);

    // Set up the new weapon variables
    psp = pp->CurWpn = pp->Wpn[pp->WeaponType];
    psp->flags |= (PANF_WEAPON_SPRITE);
    psp->ActionState = ps_ShotgunFire;
    //psp->ActionState = ps_ShotgunAutoFire;
    psp->RetractState = ps_RetractShotgun;
    psp->PresentState = ps_PresentShotgun;
    psp->RestState = ps_ShotgunRest;
    pSetState(psp, psp->PresentState);

    PlaySound(DIGI_SHOTGUN_UP, pp, v3df_follow);

    psp->PlayerP->KeyPressBits |= SB_FIRE;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pShotgunSetRecoil(DPanelSprite* psp)
{
    psp->vel = 900;
    psp->ang = NORM_ANGLE(-256);
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pShotgunRecoilDown(DPanelSprite* psp)
{
    int targetvel = psp->PlayerP->WpnShotgunType == 1 ? 890 : 780;

    psp->backupcoords();

    psp->pos.X += pspCosVel(psp);
    psp->pos.Y -= pspSinVel(psp);

    psp->vel -= 24 * synctics;

    if (psp->vel < targetvel)
    {
        psp->vel = targetvel;
        psp->ang = NORM_ANGLE(psp->ang + 1024);

        pStatePlusOne(psp);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pShotgunRecoilUp(DPanelSprite* psp)
{
    psp->backupcoords();

    psp->pos.X += pspCosVel(psp);
    psp->pos.Y -= pspSinVel(psp);

    psp->vel += 15 * synctics;

    if (psp->pos.Y < SHOTGUN_YOFF)
    {
        psp->opos.Y = psp->pos.Y = SHOTGUN_YOFF;
        psp->opos.X = psp->pos.X = SHOTGUN_XOFF;

        pShotgunSetRecoil(psp);

        pStatePlusOne(psp);
        psp->flags &= ~(PANF_BOB);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

#if 1
void pShotgunReloadDown(DPanelSprite* psp)
{
    psp->backupy();
    psp->pos.Y += 3 * synctics;

    if (psp->pos.Y >= SHOTGUN_YOFF + (pspheight(psp)/2))
    {
        PlaySound(DIGI_ROCKET_UP, psp->PlayerP,v3df_follow|v3df_dontpan|v3df_doppler);

        psp->opos.Y = psp->pos.Y = SHOTGUN_YOFF + (pspheight(psp)/2);

        pStatePlusOne(psp);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pShotgunReloadUp(DPanelSprite* psp)
{
    psp->opos.X = psp->pos.X = SHOTGUN_XOFF;
    psp->backupy();
    psp->pos.Y -= 3 * synctics;

    psp->PlayerP->WpnReloadState = 2;

    if (psp->pos.Y < SHOTGUN_YOFF)
    {
        PlaySound(DIGI_SHOTGUN_UP, psp->PlayerP,v3df_follow|v3df_dontpan|v3df_doppler);

        psp->opos.Y = psp->pos.Y = SHOTGUN_YOFF;

        pStatePlusOne(psp);
        psp->flags &= ~(PANF_BOB);
    }
}
#endif

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pShotgunPresent(DPanelSprite* psp)
{
    if (psp->PlayerP->Flags & (PF_WEAPON_RETRACT))
        return;

    // Needed for recoil
    psp->ang = NORM_ANGLE(256 + 128);
    pShotgunSetRecoil(psp);
    ///

    psp->backupy();
    psp->pos.Y -= 3 * synctics;

    if (psp->pos.Y < SHOTGUN_YOFF)
    {
        psp->opos.Y = psp->pos.Y = SHOTGUN_YOFF;
        psp->backupboby();
        pSetState(psp, psp->RestState);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pShotgunBobSetup(DPanelSprite* psp)
{
    if (psp->flags & (PANF_BOB))
        return;

    psp->backupbobcoords();

    psp->sin_amt = 12;
    psp->sin_ndx = 0;
    psp->bob_height_divider = 8;
}

bool pShotgunOverlays(DPanelSprite* psp)
{
#define SHOTGUN_AUTO_XOFF 28
#define SHOTGUN_AUTO_YOFF -17

    if (psp->over[SHOTGUN_AUTO_NUM].xoff == -1)
    {
        psp->over[SHOTGUN_AUTO_NUM].xoff = SHOTGUN_AUTO_XOFF;
        psp->over[SHOTGUN_AUTO_NUM].yoff = SHOTGUN_AUTO_YOFF;
    }

    //if(psp->PlayerP->WpnShotgunAuto == 0 && psp->PlayerP->WpnRocketType == 1)
    //psp->PlayerP->WpnShotgunType--;

    switch (psp->PlayerP->WpnShotgunType)
    {
    case 0:
        psp->over[SHOTGUN_AUTO_NUM].pic = -1;
        psp->over[SHOTGUN_AUTO_NUM].flags |= (psf_ShadeNone);
        return false;
    case 1:
        psp->over[SHOTGUN_AUTO_NUM].pic = SHOTGUN_AUTO;
        psp->over[SHOTGUN_AUTO_NUM].flags |= (psf_ShadeNone);
        return false;
    }
    return false;
}

PANEL_STATE ps_ShotgunFlash[] =
{
    {SHOTGUN_AUTO, 30, nullptr, &ps_ShotgunFlash[1], 0,0,0},
    {0,            30, nullptr, &ps_ShotgunFlash[2], 0,0,0},
    {SHOTGUN_AUTO, 30, nullptr, &ps_ShotgunFlash[3], 0,0,0},
    {0,            30, nullptr, &ps_ShotgunFlash[4], 0,0,0},
    {SHOTGUN_AUTO, 30, nullptr, &ps_ShotgunFlash[5], 0,0,0},
    {0,            30, nullptr, &ps_ShotgunFlash[6], 0,0,0},
    {SHOTGUN_AUTO, 30, nullptr, &ps_ShotgunFlash[7], 0,0,0},
    {0,            30, nullptr, &ps_ShotgunFlash[8], 0,0,0},
    {SHOTGUN_AUTO, 30, nullptr, &ps_ShotgunFlash[9], 0,0,0},
    {0,             0, nullptr, nullptr, 0,0,0}
};


//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pShotgunHide(DPanelSprite* psp)
{
    psp->backupy();
    psp->pos.Y += 3 * synctics;

    if (psp->pos.Y >= SHOTGUN_YOFF + pspheight(psp))
    {
        psp->opos.Y = psp->pos.Y = SHOTGUN_YOFF + pspheight(psp);

        pWeaponUnHideKeys(psp, psp->PresentState);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

#if 1
bool pShotgunReloadTest(DPanelSprite* psp)
{
    //short ammo = psp->PlayerP->WpnAmmo[psp->PlayerP->WeaponType];
    short ammo = psp->PlayerP->WpnAmmo[WPN_SHOTGUN];

    // Reload if done with clip
    if (ammo > 0 && (ammo % 4) == 0)
    {
        // clip has run out
        psp->flags &= ~(PANF_REST_POS);
        pSetState(psp, ps_ShotgunReload);
        return true;
    }

    return false;
}
#endif

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pShotgunRest(DPanelSprite* psp)
{
    bool force = !!(psp->flags & PANF_UNHIDE_SHOOT);
    //short ammo = psp->PlayerP->WpnAmmo[psp->PlayerP->WeaponType];
    int ammo = psp->PlayerP->WpnAmmo[WPN_SHOTGUN];
    int lastammo = psp->PlayerP->WpnShotgunLastShell;

    if (pWeaponHideKeys(psp, ps_ShotgunHide))
        return;

    if (psp->PlayerP->WpnShotgunType == 1 && ammo > 0 && ((ammo % 4) != 0) && lastammo != ammo && (psp->flags & PANF_REST_POS))
    {
        force = true;
    }

    pShotgunOverlays(psp);

    pShotgunBobSetup(psp);
    pWeaponBob(psp, PLAYER_MOVING(psp->PlayerP));


    if ((psp->PlayerP->cmd.ucmd.actions & SB_FIRE) || force)
    {
        if ((psp->PlayerP->KeyPressBits & SB_FIRE) || force)
        {
            psp->flags &= ~(PANF_UNHIDE_SHOOT);

            if (!WeaponOK(psp->PlayerP))
                return;

            psp->flags |= (PANF_REST_POS); // Used for reload checking in autofire

            if (psp->PlayerP->WpnShotgunType == 0)
                psp->PlayerP->WpnShotgunLastShell = ammo-1;

            DoPlayerChooseYell(psp->PlayerP);
            if (psp->PlayerP->WpnShotgunType==0)
                pSetState(psp, ps_ShotgunFire);
            else
                pSetState(psp, ps_ShotgunAutoFire);
        }
        if (!WeaponOK(psp->PlayerP))
            return;
    }
    else
        WeaponOK(psp->PlayerP);
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pShotgunRestTest(DPanelSprite* psp)
{
    bool force = !!(psp->flags & PANF_UNHIDE_SHOOT);

    if (psp->PlayerP->WpnShotgunType == 1 && !pShotgunReloadTest(psp))
        force = true;

    if (pShotgunReloadTest(psp))
        return;

    if (pWeaponHideKeys(psp, ps_ShotgunHide))
        return;

    pShotgunBobSetup(psp);
    pWeaponBob(psp, PLAYER_MOVING(psp->PlayerP));

    if ((psp->PlayerP->cmd.ucmd.actions & SB_FIRE) || force)
    {
        if ((psp->PlayerP->KeyPressBits & SB_FIRE) || force)
        {
            psp->flags &= ~(PANF_UNHIDE_SHOOT);

            if (!WeaponOK(psp->PlayerP))
                return;

            DoPlayerChooseYell(psp->PlayerP);
            return;
        }
    }

    pSetState(psp, psp->RestState);
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pShotgunAction(DPanelSprite* psp)
{
    pShotgunBobSetup(psp);
    pWeaponBob(psp, PLAYER_MOVING(psp->PlayerP));
}


void pShotgunFire(DPanelSprite* psp)
{
    SpawnVis(psp->PlayerP->GetActor(), nullptr, {}, 32);
    InitShotgun(psp->PlayerP);
    //SpawnShotgunShell(psp);
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pShotgunRetract(DPanelSprite* psp)
{
    psp->backupy();
    psp->pos.Y += 3 * synctics;

    if (psp->pos.Y >= SHOTGUN_YOFF + pspheight(psp) + 50)
    {
        psp->PlayerP->Flags &= ~(PF_WEAPON_RETRACT);
        psp->PlayerP->Wpn[psp->WeaponType] = nullptr;
        pKillSprite(psp);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// RAIL
//
//////////////////////////////////////////////////////////////////////////////////////////

void pRailPresent(DPanelSprite* psp);
void pRailRetract(DPanelSprite* psp);
void pRailAction(DPanelSprite* psp);
void pRailRest(DPanelSprite* psp);
void pRailFire(DPanelSprite* psp);
void pRailHide(DPanelSprite* psp);
void pRailRestTest(DPanelSprite* psp);
void pRailOkTest(DPanelSprite* psp);
void pRailRecoilUp(DPanelSprite* psp);
void pRailRecoilDown(DPanelSprite* psp);

void pRailBobSetup(DPanelSprite* psp);

bool pRailReloadTest(DPanelSprite* psp);

#define Rail_BEAT_RATE 24
#define Rail_ACTION_RATE 3  // !JIM! Was 10
#define Rail_CHARGE_RATE 0

PANEL_STATE ps_PresentRail[] =
{
    {ID_RailPresent0, Rail_BEAT_RATE, pRailPresent, &ps_PresentRail[0], psf_ShadeNone, 0,0}
};

PANEL_STATE ps_RailRest[] =
{
    {ID_RailRest0, Rail_BEAT_RATE, pRailRest, &ps_RailRest[1], psf_ShadeNone, 0,0},
    {ID_RailRest1, Rail_BEAT_RATE, pRailRest, &ps_RailRest[2], psf_ShadeNone, 0,0},
    {ID_RailRest2, Rail_BEAT_RATE, pRailRest, &ps_RailRest[3], psf_ShadeNone, 0,0},
    {ID_RailRest3, Rail_BEAT_RATE, pRailRest, &ps_RailRest[4], psf_ShadeNone, 0,0},
    {ID_RailRest4, Rail_BEAT_RATE, pRailRest, &ps_RailRest[0], psf_ShadeNone, 0,0},
};

PANEL_STATE ps_RailHide[] =
{
    {ID_RailPresent0, Rail_BEAT_RATE, pRailHide, &ps_RailHide[0], psf_ShadeNone, 0,0}
};

PANEL_STATE ps_RailRecoil[] =
{
    // recoil
    {ID_RailPresent0, Rail_BEAT_RATE, pRailRecoilDown, &ps_RailRecoil[0], 0,0,0},
    {ID_RailPresent0, Rail_BEAT_RATE, pRailRecoilUp, &ps_RailRecoil[1], 0,0,0},
    // ready to fire again
    {ID_RailPresent0, 3, pNullAnimator, &ps_RailRest[0], 0,0,0}
};

PANEL_STATE ps_RailFire[] =
{
    {ID_RailCharge0,       Rail_CHARGE_RATE, pRailAction,    &ps_RailFire[1], psf_ShadeNone, 0,0},
    {ID_RailCharge1,       Rail_CHARGE_RATE, pRailAction,    &ps_RailFire[2], psf_ShadeNone, 0,0},
    {ID_RailCharge2,       Rail_CHARGE_RATE, pRailAction,    &ps_RailFire[3], psf_ShadeNone, 0,0},
    {ID_RailCharge1,       Rail_CHARGE_RATE, pRailAction,    &ps_RailFire[4], psf_ShadeNone, 0,0},

    {ID_RailFire0,       Rail_ACTION_RATE, pRailAction,    &ps_RailFire[5], psf_ShadeNone, 0,0},
    {ID_RailFire1,       Rail_ACTION_RATE, pRailAction,    &ps_RailFire[6], psf_ShadeNone, 0,0},
    {ID_RailFire1,       Rail_ACTION_RATE, pRailAction,    &ps_RailFire[7], psf_ShadeNone, 0,0},
    {ID_RailFire1,       0,                pRailFire,      &ps_RailFire[8], psf_ShadeNone|psf_QuickCall, 0,0},

    // recoil
    {ID_RailPresent0,      Rail_BEAT_RATE, pRailRecoilDown,  &ps_RailFire[8], psf_ShadeNone, 0,0},
    {ID_RailPresent0,      Rail_BEAT_RATE, pRailRecoilUp,    &ps_RailFire[9], psf_ShadeNone, 0,0},
    // !JIM! I added these to introduce firing delay, that looks like a charge down.
    {ID_RailCharge0,       Rail_CHARGE_RATE, pRailOkTest,    &ps_RailFire[11], psf_ShadeNone, 0,0},
    {ID_RailCharge1,       Rail_CHARGE_RATE, pRailAction,    &ps_RailFire[12], psf_ShadeNone, 0,0},
    {ID_RailCharge2,       Rail_CHARGE_RATE, pRailOkTest,    &ps_RailFire[13], psf_ShadeNone, 0,0},
    {ID_RailCharge1,       Rail_CHARGE_RATE, pRailAction,    &ps_RailFire[14], psf_ShadeNone, 0,0},
    {ID_RailCharge0,       Rail_CHARGE_RATE+1, pRailAction,    &ps_RailFire[15], psf_ShadeNone, 0,0},
    {ID_RailCharge1,       Rail_CHARGE_RATE+1, pRailAction,    &ps_RailFire[16], psf_ShadeNone, 0,0},
    {ID_RailCharge2,       Rail_CHARGE_RATE+1, pRailAction,    &ps_RailFire[17], psf_ShadeNone, 0,0},
    {ID_RailCharge1,       Rail_CHARGE_RATE+2, pRailAction,    &ps_RailFire[18], psf_ShadeNone, 0,0},
    {ID_RailCharge0,       Rail_CHARGE_RATE+2, pRailAction,    &ps_RailFire[19], psf_ShadeNone, 0,0},
    {ID_RailCharge1,       Rail_CHARGE_RATE+2, pRailAction,    &ps_RailFire[20], psf_ShadeNone, 0,0},
    {ID_RailCharge2,       Rail_CHARGE_RATE+3, pRailAction,    &ps_RailFire[21], psf_ShadeNone, 0,0},
    {ID_RailCharge1,       Rail_CHARGE_RATE+3, pRailAction,    &ps_RailFire[22], psf_ShadeNone, 0,0},
    {ID_RailCharge0,       Rail_CHARGE_RATE+4, pRailAction,    &ps_RailFire[23], psf_ShadeNone, 0,0},
    {ID_RailCharge1,       Rail_CHARGE_RATE+4, pRailAction,    &ps_RailFire[24], psf_ShadeNone, 0,0},
    {ID_RailCharge2,       Rail_CHARGE_RATE+4, pRailAction,    &ps_RailFire[25], psf_ShadeNone, 0,0},
    {ID_RailCharge0,       Rail_CHARGE_RATE+5, pRailAction,    &ps_RailFire[26], psf_ShadeNone, 0,0},
    {ID_RailCharge1,       Rail_CHARGE_RATE+5, pRailAction,    &ps_RailFire[27], psf_ShadeNone, 0,0},
    {ID_RailCharge2,       Rail_CHARGE_RATE+5, pRailAction,    &ps_RailFire[28], psf_ShadeNone, 0,0},

    {ID_RailCharge0,      Rail_ACTION_RATE, pRailRestTest,  &ps_RailFire[29], psf_ShadeNone, 0,0},
    {ID_RailCharge1,      Rail_ACTION_RATE, pRailRest,      &ps_RailRest[0], psf_ShadeNone, 0,0},
};

PANEL_STATE ps_RailFireEMP[] =
{
    {ID_RailCharge0,       Rail_CHARGE_RATE, pRailAction,    &ps_RailFireEMP[1], psf_ShadeNone, 0,0},
    {ID_RailCharge1,       Rail_CHARGE_RATE, pRailAction,    &ps_RailFireEMP[2], psf_ShadeNone, 0,0},
    {ID_RailCharge2,       Rail_CHARGE_RATE, pRailAction,    &ps_RailFireEMP[3], psf_ShadeNone, 0,0},
    {ID_RailCharge1,       Rail_CHARGE_RATE, pRailAction,    &ps_RailFireEMP[4], psf_ShadeNone, 0,0},

    {ID_RailFire0,       Rail_ACTION_RATE, pRailAction,    &ps_RailFireEMP[5], psf_ShadeNone, 0,0},
    {ID_RailFire1,       Rail_ACTION_RATE, pRailAction,    &ps_RailFireEMP[6], psf_ShadeNone, 0,0},
    {ID_RailFire1,       Rail_ACTION_RATE, pRailAction,    &ps_RailFireEMP[7], psf_ShadeNone, 0,0},
    {ID_RailFire1,       0,                pRailFire,      &ps_RailFireEMP[8], psf_ShadeNone|psf_QuickCall, 0,0},

    {ID_RailCharge0,      Rail_ACTION_RATE, pRailRestTest,  &ps_RailFireEMP[9], psf_ShadeNone, 0,0},
    {ID_RailCharge1,      Rail_ACTION_RATE, pRailRest,      &ps_RailRest[0], psf_ShadeNone, 0,0},
};


PANEL_STATE ps_RetractRail[] =
{
    {ID_RailPresent0, Rail_BEAT_RATE, pRailRetract, &ps_RetractRail[0], psf_ShadeNone, 0,0}
};

#define RAIL_YOFF 200
//#define RAIL_XOFF (160+60)
#define RAIL_XOFF (160+6)

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void InitWeaponRail(DSWPlayer* pp)
{
    DPanelSprite* psp = nullptr;

    if (SW_SHAREWARE) return;

    if (Prediction)
        return;

    pp->WeaponType = WPN_RAIL;

    if (!(pp->WpnFlags &BIT(pp->WeaponType)) ||
//        pp->WpnAmmo[pp->WeaponType] <= 0 ||
        (pp->Flags & PF_WEAPON_RETRACT))
        return;

    if (!pp->Wpn[pp->WeaponType])
    {
        psp = pp->Wpn[pp->WeaponType] = pSpawnSprite(pp, ps_PresentRail, PRI_MID, RAIL_XOFF, RAIL_YOFF);
        psp->pos.Y += pspheight(psp);
        psp->backupy();
    }

    if (pp->CurWpn == pp->Wpn[pp->WeaponType])
    {
        return;
    }

    psp->WeaponType = pp->WeaponType;
    PlayerUpdateWeapon(pp, pp->WeaponType);

    pp->WpnUziType = 2; // Make uzi's go away!
    RetractCurWpn(pp);

    // Set up the new weapon variables
    psp = pp->CurWpn = pp->Wpn[pp->WeaponType];
    psp->flags |= (PANF_WEAPON_SPRITE);
    psp->ActionState = ps_RailFire;
    psp->RetractState = ps_RetractRail;
    psp->PresentState = ps_PresentRail;
    psp->RestState = ps_RailRest;
    pSetState(psp, psp->PresentState);

    PlaySound(DIGI_RAIL_UP, pp, v3df_follow);
    PlaySound(DIGI_RAILREADY, pp, v3df_follow | v3df_dontpan, CHAN_ITEM); // this one needs to be on a dedicated channel to allow switching it off without too many checks.

    psp->PlayerP->KeyPressBits |= SB_FIRE;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pRailSetRecoil(DPanelSprite* psp)
{
    psp->vel = 900;
    psp->ang = NORM_ANGLE(-256);
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pRailRecoilDown(DPanelSprite* psp)
{
    psp->backupcoords();

    psp->pos.X += pspCosVel(psp);
    psp->pos.Y -= pspSinVel(psp);

    psp->vel -= 24 * synctics;

    if (psp->vel < 800)
    {
        psp->vel = 800;
        psp->ang = NORM_ANGLE(psp->ang + 1024);

        pStatePlusOne(psp);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pRailRecoilUp(DPanelSprite* psp)
{
    psp->backupcoords();

    psp->pos.X += pspCosVel(psp);
    psp->pos.Y -= pspSinVel(psp);

    psp->vel += 15 * synctics;

    if (psp->pos.Y < RAIL_YOFF)
    {
        psp->opos.Y = psp->pos.Y = RAIL_YOFF;
        psp->opos.X = psp->pos.X = RAIL_XOFF;

        pRailSetRecoil(psp);

        pStatePlusOne(psp);
        psp->flags &= ~(PANF_BOB);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pRailPresent(DPanelSprite* psp)
{
    if (psp->PlayerP->Flags & (PF_WEAPON_RETRACT))
        return;

    // Needed for recoil
    psp->ang = NORM_ANGLE(256 + 128);
    pRailSetRecoil(psp);
    ///

    psp->backupy();
    psp->pos.Y -= 3 * synctics;

    if (psp->pos.Y < RAIL_YOFF)
    {
        psp->opos.Y = psp->pos.Y = RAIL_YOFF;
        psp->backupboby();
        pSetState(psp, psp->RestState);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pRailBobSetup(DPanelSprite* psp)
{
    if (psp->flags & (PANF_BOB))
        return;

    psp->backupbobcoords();

    psp->sin_amt = 12;
    psp->sin_ndx = 0;
    psp->bob_height_divider = 8;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pRailHide(DPanelSprite* psp)
{
    psp->backupy();
    psp->pos.Y += 3 * synctics;

    if (psp->pos.Y >= RAIL_YOFF + pspheight(psp))
    {
        psp->opos.Y = psp->pos.Y = RAIL_YOFF + pspheight(psp);

        pWeaponUnHideKeys(psp, psp->PresentState);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pRailOkTest(DPanelSprite* psp)
{
    if (pWeaponHideKeys(psp, ps_RailHide))
        return;

    WeaponOK(psp->PlayerP);
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pRailRest(DPanelSprite* psp)
{
    bool force = !!(psp->flags & PANF_UNHIDE_SHOOT);

    if (SW_SHAREWARE) return;

    if (pWeaponHideKeys(psp, ps_RailHide))
        return;

    pRailBobSetup(psp);
    pWeaponBob(psp, PLAYER_MOVING(psp->PlayerP));

    if ((psp->PlayerP->cmd.ucmd.actions & SB_FIRE) || force)
    {
        if ((psp->PlayerP->KeyPressBits & SB_FIRE) || force)
        {
            psp->flags &= ~(PANF_UNHIDE_SHOOT);

            if (!WeaponOK(psp->PlayerP))
                return;

            //PlaySound(DIGI_RAILPWRUP, &psp->PlayerP->posx, &psp->PlayerP->posy, &psp->PlayerP->posz, v3df_follow);

            DoPlayerChooseYell(psp->PlayerP);
            if (psp->PlayerP->WpnRailType == 0)
                pSetState(psp, ps_RailFire);
            else
                pSetState(psp, ps_RailFireEMP);
        }
    }
    else
        WeaponOK(psp->PlayerP);
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pRailRestTest(DPanelSprite* psp)
{
    bool force = !!(psp->flags & PANF_UNHIDE_SHOOT);

    if (pWeaponHideKeys(psp, ps_RailHide))
        return;

    pRailBobSetup(psp);
    pWeaponBob(psp, PLAYER_MOVING(psp->PlayerP));

    if ((psp->PlayerP->cmd.ucmd.actions & SB_FIRE) || force)
    {
        if ((psp->PlayerP->KeyPressBits & SB_FIRE) || force)
        {
            psp->flags &= ~(PANF_UNHIDE_SHOOT);

            if (!WeaponOK(psp->PlayerP))
                return;

            DoPlayerChooseYell(psp->PlayerP);
            return;
        }
    }

    pSetState(psp, psp->RestState);
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pRailAction(DPanelSprite* psp)
{
    pRailBobSetup(psp);
    pWeaponBob(psp, PLAYER_MOVING(psp->PlayerP));
}


//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pRailFire(DPanelSprite* psp)
{
    SpawnVis(psp->PlayerP->GetActor(), nullptr, {}, 16);
    InitRail(psp->PlayerP);
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pRailRetract(DPanelSprite* psp)
{
    psp->backupy();
    psp->pos.Y += 3 * synctics;

    if (psp->pos.Y >= RAIL_YOFF + pspheight(psp) + 50)
    {
        psp->PlayerP->Flags &= ~(PF_WEAPON_RETRACT);
        psp->PlayerP->Wpn[psp->WeaponType] = nullptr;
        DeleteNoSoundOwner(psp->PlayerP->GetActor());
        pKillSprite(psp);
    }
}


//////////////////////////////////////////////////////////////////////////////////////////
//
// HOTHEAD
//
//////////////////////////////////////////////////////////////////////////////////////////

void pHotheadPresent(DPanelSprite* psp);
void pHotheadRetract(DPanelSprite* psp);
void pHotheadAction(DPanelSprite* psp);
void pHotheadActionCenter(DPanelSprite* psp);
void pHotheadRest(DPanelSprite* psp);
void pHotheadRestCenter(DPanelSprite* psp);
void pHotheadAttack(DPanelSprite* psp);
void pHotheadRing(DPanelSprite* psp);
void pHotheadNapalm(DPanelSprite* psp);
void pHotheadHide(DPanelSprite* psp);
void pHotheadRestTest(DPanelSprite* psp);

extern PANEL_STATE ps_HotheadAttack[];
extern PANEL_STATE ps_ReloadHothead[];
extern PANEL_STATE ps_HotheadTurn[];


#define Hothead_BEAT_RATE 24
#define Hothead_ACTION_RATE_PRE 2  // !JIM! Was 1
#define Hothead_ACTION_RATE_POST 7

PANEL_STATE ps_PresentHothead[] =
{
    {ID_HotheadPresent0, Hothead_BEAT_RATE, pHotheadPresent, &ps_PresentHothead[0], 0,0,0}
};

PANEL_STATE ps_HotheadHide[] =
{
    {ID_HotheadRest0, Hothead_BEAT_RATE, pHotheadHide, &ps_HotheadHide[0], 0,0,0}
};

PANEL_STATE ps_RetractHothead[] =
{
    {ID_HotheadPresent0, Hothead_BEAT_RATE, pHotheadRetract, &ps_RetractHothead[0], 0,0,0}
};

PANEL_STATE ps_HotheadRest[] =
{
    {ID_HotheadRest0, Hothead_BEAT_RATE, pHotheadRest, &ps_HotheadRest[0], 0,0,0}
};

PANEL_STATE ps_HotheadRestRing[] =
{
    {ID_HotheadRest0, Hothead_BEAT_RATE, pHotheadRest, &ps_HotheadRest[0], 0,0,0}
};

PANEL_STATE ps_HotheadRestNapalm[] =
{
    {ID_HotheadRest0, Hothead_BEAT_RATE, pHotheadRest, &ps_HotheadRest[0], 0,0,0}
};
// Turns - attacks

PANEL_STATE ps_HotheadAttack[] =
{
    {ID_HotheadAttack0, Hothead_ACTION_RATE_PRE,  pHotheadAction, &ps_HotheadAttack[1], psf_ShadeHalf, 0,0},
    {ID_HotheadAttack0, 3,                         pHotheadAction, &ps_HotheadAttack[2], psf_ShadeHalf, 0,0},
    {ID_HotheadAttack0, 0,                         pHotheadAttack, &ps_HotheadAttack[3], psf_QuickCall, 0,0},
    {ID_HotheadAttack0, 3,                         pHotheadAction, &ps_HotheadAttack[4], psf_ShadeHalf, 0,0},
    {ID_HotheadAttack0, 0,                         pHotheadRestTest, &ps_HotheadAttack[4], psf_QuickCall, 0,0},
    {ID_HotheadAttack0, 0,                         pHotheadAction, &ps_HotheadAttack[0], psf_ShadeHalf, 0,0}
};

PANEL_STATE ps_HotheadRing[] =
{
    {ID_HotheadAttack0, Hothead_ACTION_RATE_PRE,  pHotheadAction, &ps_HotheadRing[1], psf_ShadeHalf, 0,0},
    {ID_HotheadAttack0, 10,                        pHotheadAction, &ps_HotheadRing[2], psf_ShadeHalf, 0,0},
    {ID_HotheadAttack0, 0,                         pHotheadAttack, &ps_HotheadRing[3], psf_QuickCall, 0,0},
    {ID_HotheadAttack0, 40,                        pHotheadAction, &ps_HotheadRing[4], psf_ShadeHalf, 0,0},
    {ID_HotheadAttack0, 0,                         pHotheadRestTest, &ps_HotheadRing[4], psf_QuickCall, 0,0},
    {ID_HotheadAttack0, 3,                         pHotheadAction, &ps_HotheadRing[0], psf_ShadeHalf, 0,0}
};

PANEL_STATE ps_HotheadNapalm[] =
{
    {ID_HotheadAttack0, Hothead_ACTION_RATE_PRE,  pHotheadAction, &ps_HotheadNapalm[1], psf_ShadeHalf, 0,0},
    {ID_HotheadAttack0, 3,                        pHotheadAction, &ps_HotheadNapalm[2], psf_ShadeHalf, 0,0},
    {ID_HotheadAttack0, 0,                         pHotheadAttack, &ps_HotheadNapalm[3], psf_QuickCall, 0,0},
    {ID_HotheadAttack0, 50,                        pHotheadAction, &ps_HotheadNapalm[4], psf_ShadeHalf, 0,0},
    {ID_HotheadAttack0, 0,                         pHotheadRestTest, &ps_HotheadNapalm[4], psf_QuickCall, 0,0},
    {ID_HotheadAttack0, 3,                         pHotheadAction, &ps_HotheadNapalm[0], psf_ShadeHalf, 0,0}
};

// Turns - can do three different turns

PANEL_STATE ps_HotheadTurn[] =
{
    {ID_HotheadTurn0, Hothead_BEAT_RATE, pNullAnimator, &ps_HotheadTurn[1], 0,0,0},
    {ID_HotheadTurn1, Hothead_BEAT_RATE, pNullAnimator, &ps_HotheadTurn[2], 0,0,0},
    {ID_HotheadTurn2, Hothead_BEAT_RATE, pNullAnimator, &ps_HotheadTurn[3], 0,0,0},
    {ID_HotheadTurn3, Hothead_BEAT_RATE, pNullAnimator, &ps_HotheadTurn[4], 0,0,0},
    {ID_HotheadChomp0, Hothead_BEAT_RATE, pNullAnimator, &ps_HotheadTurn[5], 0,0,0},
    {ID_HotheadTurn3, Hothead_BEAT_RATE, pNullAnimator, &ps_HotheadTurn[6], 0,0,0},
    {ID_HotheadChomp0, Hothead_BEAT_RATE, pNullAnimator, &ps_HotheadTurn[7], 0,0,0},
    {ID_HotheadTurn3, Hothead_BEAT_RATE, pNullAnimator, &ps_HotheadTurn[8], 0,0,0},
    {ID_HotheadTurn2, Hothead_BEAT_RATE, pNullAnimator, &ps_HotheadTurn[9], 0,0,0},
    {ID_HotheadTurn1, Hothead_BEAT_RATE, pNullAnimator, &ps_HotheadTurn[10], 0,0,0},
    {ID_HotheadTurn0, Hothead_BEAT_RATE, pNullAnimator, &ps_HotheadTurn[11], 0,0,0},
    {ID_HotheadTurn0, Hothead_BEAT_RATE, pNullAnimator, &ps_HotheadRest[0], 0,0,0}
};

PANEL_STATE ps_HotheadTurnRing[] =
{
    {ID_HotheadTurn0, Hothead_BEAT_RATE, pNullAnimator, &ps_HotheadTurnRing[1], 0,0,0},
    {ID_HotheadTurn1, Hothead_BEAT_RATE, pNullAnimator, &ps_HotheadTurnRing[2], 0,0,0},
    {ID_HotheadTurn2, Hothead_BEAT_RATE, pNullAnimator, &ps_HotheadTurnRing[3], 0,0,0},
    {ID_HotheadTurn3, Hothead_BEAT_RATE, pNullAnimator, &ps_HotheadTurnRing[4], 0,0,0},
    {ID_HotheadChomp0, Hothead_BEAT_RATE, pNullAnimator, &ps_HotheadTurnRing[5], 0,0,0},
    {ID_HotheadTurn3, Hothead_BEAT_RATE, pNullAnimator, &ps_HotheadTurnRing[6], 0,0,0},
    {ID_HotheadTurn2, Hothead_BEAT_RATE, pNullAnimator, &ps_HotheadTurnRing[7], 0,0,0},
    {ID_HotheadTurn1, Hothead_BEAT_RATE, pNullAnimator, &ps_HotheadTurnRing[8], 0,0,0},
    {ID_HotheadTurn0, Hothead_BEAT_RATE, pNullAnimator, &ps_HotheadTurnRing[9], 0,0,0},
    {ID_HotheadTurn0, Hothead_BEAT_RATE, pNullAnimator, &ps_HotheadRestRing[0], 0,0,0}
};

PANEL_STATE ps_HotheadTurnNapalm[] =
{
    {ID_HotheadTurn0, Hothead_BEAT_RATE, pNullAnimator, &ps_HotheadTurnNapalm[1], 0,0,0},
    {ID_HotheadTurn1, Hothead_BEAT_RATE, pNullAnimator, &ps_HotheadTurnNapalm[2], 0,0,0},
    {ID_HotheadTurn2, Hothead_BEAT_RATE, pNullAnimator, &ps_HotheadTurnNapalm[3], 0,0,0},
    {ID_HotheadTurn3, Hothead_BEAT_RATE, pNullAnimator, &ps_HotheadTurnNapalm[4], 0,0,0},
    {ID_HotheadTurn3, Hothead_BEAT_RATE*2, pNullAnimator, &ps_HotheadTurnNapalm[5], 0,0,0},
    {ID_HotheadTurn2, Hothead_BEAT_RATE, pNullAnimator, &ps_HotheadTurnNapalm[6], 0,0,0},
    {ID_HotheadTurn1, Hothead_BEAT_RATE, pNullAnimator, &ps_HotheadTurnNapalm[7], 0,0,0},
    {ID_HotheadTurn0, Hothead_BEAT_RATE, pNullAnimator, &ps_HotheadTurnNapalm[8], 0,0,0},
    {ID_HotheadTurn0, Hothead_BEAT_RATE, pNullAnimator, &ps_HotheadRestNapalm[0], 0,0,0}
};


PANEL_STATE* HotheadAttackStates[] =
{
    ps_HotheadAttack,
    ps_HotheadRing,
    ps_HotheadNapalm
};

PANEL_STATE* HotheadRestStates[] =
{
    ps_HotheadRest,
    ps_HotheadRestRing,
    ps_HotheadRestNapalm
};

PANEL_STATE* HotheadTurnStates[] =
{
    ps_HotheadTurn,
    ps_HotheadTurnRing,
    ps_HotheadTurnNapalm
};

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

#define FIREBALL_MODE 0
#define     RING_MODE 1
#define   NAPALM_MODE 2
void pHotHeadOverlays(DPanelSprite* psp, short mode)
{
#define HOTHEAD_FINGER_XOFF 0
#define HOTHEAD_FINGER_YOFF 0

    switch (mode)
    {
    case 0: // Great balls o' fire
        psp->over[0].pic = HEAD_MODE1;
        break;
    case 1: // Ring of fire
        psp->over[0].pic = HEAD_MODE2;
        break;
    case 2: // I love the smell of napalm in the morning
        psp->over[0].pic = HEAD_MODE3;
        break;
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

#define HOTHEAD_BOB_X_AMT 10

#define HOTHEAD_XOFF (200 + HOTHEAD_BOB_X_AMT + 6)
#define HOTHEAD_YOFF 200

void InitWeaponHothead(DSWPlayer* pp)
{
    DPanelSprite* psp = nullptr;

    if (SW_SHAREWARE) return;

    if (Prediction)
        return;

    if (!(pp->WpnFlags &BIT(WPN_HOTHEAD)) ||
//        pp->WpnAmmo[WPN_HOTHEAD] <= 0 ||
        (pp->Flags & PF_WEAPON_RETRACT))
        return;

    if (!pp->Wpn[WPN_HOTHEAD])
    {
        psp = pp->Wpn[WPN_HOTHEAD] = pSpawnSprite(pp, ps_PresentHothead, PRI_MID, HOTHEAD_XOFF, HOTHEAD_YOFF);
        psp->pos.Y += pspheight(psp);
        psp->backupy();
    }

    if (pp->CurWpn == pp->Wpn[WPN_HOTHEAD])
    {
        return;
    }

    psp->WeaponType = WPN_HOTHEAD;
    PlayerUpdateWeapon(pp, WPN_HOTHEAD);

    pp->WpnUziType = 2; // Make uzi's go away!
    RetractCurWpn(pp);

    // Set up the new weapon variables
    psp = pp->CurWpn = pp->Wpn[WPN_HOTHEAD];
    psp->flags |= (PANF_WEAPON_SPRITE);
    psp->ActionState = ps_HotheadAttack;
    psp->PresentState = ps_PresentHothead;
    psp->RestState = HotheadRestStates[psp->PlayerP->WpnFlameType];
    psp->RetractState = ps_RetractHothead;
    pSetState(psp, psp->PresentState);
    psp->ang = 768;
    psp->vel = 512;

    psp->PlayerP->KeyPressBits |= SB_FIRE;
    pHotHeadOverlays(psp, pp->WpnFlameType);
    psp->over[0].xoff = HOTHEAD_FINGER_XOFF;
    psp->over[0].yoff = HOTHEAD_FINGER_YOFF;

    PlaySound(DIGI_GRDALERT, pp, v3df_follow|v3df_dontpan);
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pHotheadRestTest(DPanelSprite* psp)
{
    if (psp->PlayerP->cmd.ucmd.actions & SB_FIRE)
    {
        if (psp->PlayerP->KeyPressBits & SB_FIRE)
        {
            //if (!(psp->PlayerP->Flags & PF_DIVING))
            {
                if (!WeaponOK(psp->PlayerP))
                    return;

                if (psp->PlayerP->WpnAmmo[WPN_HOTHEAD] < 10)
                {
                    psp->PlayerP->WpnFlameType = 0;
                    WeaponOK(psp->PlayerP);
                }

                DoPlayerChooseYell(psp->PlayerP);
            }

            pStatePlusOne(psp);
            return;
        }
    }

    pSetState(psp, HotheadRestStates[psp->PlayerP->WpnFlameType]);
    psp->over[0].xoff = HOTHEAD_FINGER_XOFF;
    psp->over[0].yoff = HOTHEAD_FINGER_YOFF;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pHotheadPresent(DPanelSprite* psp)
{
    if (psp->PlayerP->Flags & (PF_WEAPON_RETRACT))
        return;

    psp->backupy();
    psp->pos.Y -= 3 * synctics;

    if (psp->pos.Y < HOTHEAD_YOFF)
    {
        psp->opos.Y = psp->pos.Y = HOTHEAD_YOFF;
        psp->backupboby();
        pSetState(psp, psp->RestState);
        //pSetState(psp, HotheadTurnStates[psp->PlayerP->WpnFlameType]);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pHotheadBobSetup(DPanelSprite* psp)
{
    if (psp->flags & (PANF_BOB))
        return;

    psp->backupbobcoords();

    psp->sin_amt = HOTHEAD_BOB_X_AMT;
    psp->sin_ndx = 0;
    psp->bob_height_divider = 4;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pHotheadHide(DPanelSprite* psp)
{
    auto tex = tileGetTexture(psp->picndx);

    psp->backupx();
    psp->pos.X += 3 * synctics;

    if (psp->pos.X >= HOTHEAD_XOFF + tex->GetDisplayWidth() || psp->pos.Y >= HOTHEAD_YOFF + tex->GetDisplayHeight())
    {
        psp->opos.X = psp->pos.X = HOTHEAD_XOFF;
        psp->opos.Y = psp->pos.Y = HOTHEAD_YOFF + tex->GetDisplayHeight();

        pWeaponUnHideKeys(psp, psp->PresentState);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pHotheadRest(DPanelSprite* psp)
{
    bool force = !!(psp->flags & PANF_UNHIDE_SHOOT);

    if (SW_SHAREWARE) return;

    if (pWeaponHideKeys(psp, ps_HotheadHide))
        return;

    if (HotheadRestStates[psp->PlayerP->WpnFlameType] != psp->RestState)
    {
        psp->RestState = HotheadRestStates[psp->PlayerP->WpnFlameType];
        pSetState(psp, HotheadRestStates[psp->PlayerP->WpnFlameType]);
    }

    // in rest position - only bob when in rest pos
    pHotheadBobSetup(psp);
    pWeaponBob(psp, PLAYER_MOVING(psp->PlayerP));

    if ((psp->PlayerP->cmd.ucmd.actions & SB_FIRE) || force)
    {
        if ((psp->PlayerP->KeyPressBits & SB_FIRE) || force)
        {
            psp->flags &= ~(PANF_UNHIDE_SHOOT);

            //if ((psp->PlayerP->Flags & PF_DIVING))
            //    return;

            if (!WeaponOK(psp->PlayerP))
                return;

            if (psp->PlayerP->WpnAmmo[WPN_HOTHEAD] < 10)
            {
                psp->PlayerP->WpnFlameType = 0;
                WeaponOK(psp->PlayerP);
            }

            DoPlayerChooseYell(psp->PlayerP);
            //pSetState(psp, psp->ActionState);
            pSetState(psp, HotheadAttackStates[psp->PlayerP->WpnFlameType]);
            psp->over[0].xoff = HOTHEAD_FINGER_XOFF-1;
            psp->over[0].yoff = HOTHEAD_FINGER_YOFF-10;
        }
    }
    else
        WeaponOK(psp->PlayerP);
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pHotheadAction(DPanelSprite* psp)
{
    bool shooting = (psp->PlayerP->cmd.ucmd.actions & SB_FIRE) && (psp->PlayerP->KeyPressBits & SB_FIRE);

    if (shooting)
    {
        pUziBobSetup(psp);
        pWeaponBob(psp, PLAYER_MOVING(psp->PlayerP) || shooting);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pHotheadAttack(DPanelSprite* psp)
{
    switch (psp->PlayerP->WpnFlameType)
    {
    case 0:
        SpawnVis(psp->PlayerP->GetActor(), nullptr, {}, 32);
        InitFireball(psp->PlayerP);
        break;
    case 1:
        SpawnVis(psp->PlayerP->GetActor(), nullptr, {}, 20);
        InitSpellRing(psp->PlayerP);
        break;
    case 2:
        SpawnVis(psp->PlayerP->GetActor(), nullptr, {}, 16);
        InitSpellNapalm(psp->PlayerP);
        break;
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pHotheadRetract(DPanelSprite* psp)
{
    psp->backupy();
    psp->pos.Y += 3 * synctics;

    if (psp->pos.Y >= HOTHEAD_YOFF + pspheight(psp))
    {
        psp->PlayerP->Flags &= ~(PF_WEAPON_RETRACT);
        psp->PlayerP->Wpn[WPN_HOTHEAD] = nullptr;
        pKillSprite(psp);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// HOTHEAD ON_FIRE
//
//////////////////////////////////////////////////////////////////////////////////////////

void pOnFire(DPanelSprite*);

#define ON_FIRE_RATE 10

PANEL_STATE ps_OnFire[] =
{
    {ID_OnFire0, ON_FIRE_RATE, pOnFire, &ps_OnFire[1], 0,0,0},
    {ID_OnFire1, ON_FIRE_RATE, pOnFire, &ps_OnFire[2], 0,0,0},
    {ID_OnFire2, ON_FIRE_RATE, pOnFire, &ps_OnFire[3], 0,0,0},
    {ID_OnFire3, ON_FIRE_RATE, pOnFire, &ps_OnFire[4], 0,0,0},
    {ID_OnFire4, ON_FIRE_RATE, pOnFire, &ps_OnFire[5], 0,0,0},
    {ID_OnFire5, ON_FIRE_RATE, pOnFire, &ps_OnFire[6], 0,0,0},
    {ID_OnFire6, ON_FIRE_RATE, pOnFire, &ps_OnFire[7], 0,0,0},
    {ID_OnFire7, ON_FIRE_RATE, pOnFire, &ps_OnFire[8], 0,0,0},
    {ID_OnFire8, ON_FIRE_RATE, pOnFire, &ps_OnFire[9], 0,0,0},
    {ID_OnFire9, ON_FIRE_RATE, pOnFire, &ps_OnFire[10], 0,0,0},
    {ID_OnFire10, ON_FIRE_RATE, pOnFire, &ps_OnFire[11], 0,0,0},
    {ID_OnFire11, ON_FIRE_RATE, pOnFire, &ps_OnFire[12], 0,0,0},
    {ID_OnFire12, ON_FIRE_RATE, pOnFire, &ps_OnFire[0], 0,0,0},
};

#define ON_FIRE_Y_TOP 190
#define ON_FIRE_Y_BOT 230

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void SpawnOnFire(DSWPlayer* pp)
{
    DPanelSprite* fire;
    int x = 50;

    while (x < 320)
    {
        fire = pSpawnSprite(pp, &ps_OnFire[RANDOM_P2(8<<8)>>8], PRI_FRONT, x, ON_FIRE_Y_BOT);
        fire->flags |= PANF_WEAPON_SPRITE;
        auto tex = tileGetTexture(fire->picndx);
        x += (int)tex->GetDisplayWidth();
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pOnFire(DPanelSprite* psp)
{
    DSWActor* plActor = psp->PlayerP->GetActor();

    // Kill immediately - in case of death/water
    if (plActor->user.flameActor == nullptr && plActor->user.Flags2 & SPR2_FLAMEDIE)
    {
        pKillSprite(psp);
        return;
    }

    psp->backupy();    

    if (plActor->user.flameActor == nullptr)
    {
        // take flames down and kill them
        psp->pos.Y += 1;
        if (psp->pos.Y > ON_FIRE_Y_BOT)
        {
            pKillSprite(psp);
            return;
        }
    }
    else
    {
        // bring flames up
        psp->pos.Y -= 2;
        if (psp->pos.Y < ON_FIRE_Y_TOP)
            psp->opos.Y = psp->pos.Y = ON_FIRE_Y_TOP;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// MICRO
//
//////////////////////////////////////////////////////////////////////////////////////////

void pMicroPresent(DPanelSprite* psp);
void pMicroRetract(DPanelSprite* psp);
void pMicroAction(DPanelSprite* psp);
void pMicroRest(DPanelSprite* psp);
void pMicroFire(DPanelSprite* psp);
void pMicroHide(DPanelSprite* psp);
void pMicroUnHide(DPanelSprite* psp);
void pMicroRecoilDown(DPanelSprite* psp);
void pMicroRecoilUp(DPanelSprite* psp);
void pMicroBobSetup(DPanelSprite* psp);
void pMicroReloadUp(DPanelSprite* psp);
void pMicroReloadDown(DPanelSprite* psp);
void pMicroStandBy(DPanelSprite* psp);
void pMicroCount(DPanelSprite* psp);
void pMicroReady(DPanelSprite* psp);
void pNukeAction(DPanelSprite* psp);

extern PANEL_STATE ps_MicroReload[];

#define Micro_REST_RATE 24
#define Micro_ACTION_RATE 6  // !JIM! was 9

PANEL_STATE ps_PresentMicro[] =
{
    {ID_MicroPresent0, Micro_REST_RATE, pMicroPresent, &ps_PresentMicro[0], 0,0,0}
};

PANEL_STATE ps_MicroRest[] =
{
    {ID_MicroPresent0, Micro_REST_RATE, pMicroRest, &ps_MicroRest[0], 0,0,0}
};

PANEL_STATE ps_MicroHide[] =
{
    {ID_MicroPresent0, Micro_REST_RATE, pMicroHide, &ps_MicroHide[0], 0,0,0}
};

PANEL_STATE ps_InitNuke[] =
{
    {ID_MicroPresent0, Micro_ACTION_RATE,pNukeAction,  &ps_InitNuke[1], 0,0,0},
    {ID_MicroPresent0, 0,               pMicroStandBy,  &ps_InitNuke[2], psf_QuickCall, 0,0},
    {ID_MicroPresent0, 120*2,           pNukeAction, &ps_InitNuke[3], 0,0,0},
    {ID_MicroPresent0, 0,               pMicroCount,  &ps_InitNuke[4], psf_QuickCall, 0,0},
    {ID_MicroPresent0, 120*3,           pNukeAction, &ps_InitNuke[5], 0,0,0},
    {ID_MicroPresent0, 0,               pMicroReady,  &ps_InitNuke[6], psf_QuickCall, 0,0},
    {ID_MicroPresent0, 120*2,           pNukeAction, &ps_InitNuke[7], 0,0,0},
    {ID_MicroPresent0, 3,               pNukeAction, &ps_MicroRest[0], 0,0,0}
};

PANEL_STATE ps_MicroRecoil[] =
{
    // recoil
    {ID_MicroPresent0, Micro_ACTION_RATE, pMicroRecoilDown, &ps_MicroRecoil[0], 0,0,0},
    {ID_MicroPresent0, Micro_ACTION_RATE, pMicroRecoilUp,   &ps_MicroRecoil[1], 0,0,0},

    // Firing delay.
    {ID_MicroPresent0, 30, pNullAnimator,   &ps_MicroRecoil[3], 0,0,0},
    // ready to fire again
    {ID_MicroPresent0, 3, pNullAnimator, &ps_MicroRest[0], 0,0,0}
};

PANEL_STATE ps_MicroFire[] =
{
    {ID_MicroFire0, Micro_ACTION_RATE, pMicroAction, &ps_MicroFire[1], psf_ShadeNone, 0,0},
    {ID_MicroFire1, Micro_ACTION_RATE, pMicroAction, &ps_MicroFire[2], psf_ShadeNone, 0,0},
    {ID_MicroFire2, Micro_ACTION_RATE, pMicroAction, &ps_MicroFire[3], psf_ShadeHalf, 0,0},
    {ID_MicroFire3, Micro_ACTION_RATE, pMicroAction, &ps_MicroFire[4], psf_ShadeHalf, 0,0},
    {ID_MicroPresent0, 0,              pMicroFire, &ps_MicroFire[5], psf_ShadeNone|psf_QuickCall, 0,0},


    // !JIM! After firing delay so rockets can't fire so fast!
    // Putting a BIG blast radius for rockets, this is better than small and fast for this weap.
    {ID_MicroPresent0, 120, pMicroAction, &ps_MicroFire[6], 0,0,0},

    {ID_MicroPresent0, 3, pMicroAction, &ps_MicroRecoil[0], 0,0,0}
};

#define Micro_SINGLE_RATE 8
#define Micro_DISSIPATE_RATE 6

PANEL_STATE ps_MicroSingleFire[] =
{
    {ID_MicroSingleFire0, Micro_SINGLE_RATE,    pMicroAction,   &ps_MicroSingleFire[1], psf_ShadeHalf, 0,0},
    {ID_MicroSingleFire1, Micro_SINGLE_RATE,    pMicroAction,   &ps_MicroSingleFire[2], psf_ShadeNone, 0,0},
    {ID_MicroSingleFire1, 0,                    pMicroFire,     &ps_MicroSingleFire[3], psf_ShadeNone|psf_QuickCall, 0,0},
    {ID_MicroSingleFire2, Micro_DISSIPATE_RATE, pMicroAction,   &ps_MicroSingleFire[4], psf_ShadeNone, 0,0},
    {ID_MicroSingleFire3, Micro_DISSIPATE_RATE, pMicroAction,   &ps_MicroSingleFire[5], psf_ShadeHalf, 0,0},

    // !JIM! Put in firing delay.
    //{ID_MicroPresent0, 60, pMicroAction,   &ps_MicroSingleFire[6]},

    {ID_MicroPresent0,    3,                    pMicroAction,   &ps_MicroRecoil[0], 0,0,0}
};

PANEL_STATE ps_RetractMicro[] =
{
    {ID_MicroPresent0, Micro_REST_RATE, pMicroRetract, &ps_RetractMicro[0], 0,0,0}
};

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

#define MICRO_BOB_X_AMT 10
#define MICRO_YOFF 205
#define MICRO_XOFF (150+MICRO_BOB_X_AMT)

void pMicroSetRecoil(DPanelSprite* psp)
{
    psp->vel = 900;
    psp->ang = NORM_ANGLE(-256);
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void InitWeaponMicro(DSWPlayer* pp)
{
    DPanelSprite* psp;

    if (Prediction)
        return;

    if (!(pp->WpnFlags &BIT(WPN_MICRO)) ||
//        pp->WpnAmmo[WPN_MICRO] <= 0 ||
        (pp->Flags & PF_WEAPON_RETRACT))
        return;

    if (!pp->Wpn[WPN_MICRO])
    {
        psp = pp->Wpn[WPN_MICRO] = pSpawnSprite(pp, ps_PresentMicro, PRI_MID, MICRO_XOFF, MICRO_YOFF);
        psp->pos.Y += pspheight(psp);
        psp->backupy();
    }

    if (pp->CurWpn == pp->Wpn[WPN_MICRO])
    {
        if (pp->TestNukeInit && pp->WpnRocketType == 2 && !pp->InitingNuke && pp->WpnRocketNuke && !pp->NukeInitialized)
        {
            pp->TestNukeInit = false;
            pp->InitingNuke = true;
            psp = pp->Wpn[WPN_MICRO];
            pSetState(psp, ps_InitNuke);
        }
        return;
    }

    PlayerUpdateWeapon(pp, WPN_MICRO);

    pp->WpnUziType = 2; // Make uzi's go away!
    RetractCurWpn(pp);

    // Set up the new weapon variables
    psp = pp->CurWpn = pp->Wpn[WPN_MICRO];
    psp->flags |= (PANF_WEAPON_SPRITE);
    psp->ActionState = ps_MicroFire;
    psp->RetractState = ps_RetractMicro;
    psp->RestState = ps_MicroRest;
    psp->PresentState = ps_PresentMicro;
    pSetState(psp, psp->PresentState);

    if (pp->WpnRocketType == 2 && !pp->InitingNuke && !pp->NukeInitialized)
        pp->TestNukeInit = pp->InitingNuke = true;

    PlaySound(DIGI_ROCKET_UP, pp, v3df_follow);

    psp->PlayerP->KeyPressBits |= SB_FIRE;

}


//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pMicroRecoilDown(DPanelSprite* psp)
{
    psp->backupcoords();

    psp->pos.X += pspCosVel(psp);
    psp->pos.Y -= pspSinVel(psp);

    psp->vel -= 24 * synctics;

    if (psp->vel < 550)
    {
        psp->vel = 550;
        psp->ang = NORM_ANGLE(psp->ang + 1024);

        pStatePlusOne(psp);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pMicroRecoilUp(DPanelSprite* psp)
{
    psp->backupcoords();

    psp->pos.X += pspCosVel(psp);
    psp->pos.Y -= pspSinVel(psp);

    psp->vel += 15 * synctics;

    if (psp->pos.Y < MICRO_YOFF)
    {
        psp->opos.Y = psp->pos.Y = MICRO_YOFF;
        psp->opos.X = psp->pos.X = MICRO_XOFF;

        pMicroSetRecoil(psp);

        pStatePlusOne(psp);
        psp->flags &= ~(PANF_BOB);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pMicroPresent(DPanelSprite* psp)
{
    DSWPlayer* pp = psp->PlayerP;

    if (psp->PlayerP->Flags & (PF_WEAPON_RETRACT))
        return;

    // Needed for recoil
    psp->ang = NORM_ANGLE(256 + 96);
    pMicroSetRecoil(psp);
    ///

    psp->backupy();
    psp->pos.Y -= 3 * synctics;

    if (psp->pos.Y < MICRO_YOFF)
    {
        psp->opos.Y = psp->pos.Y = MICRO_YOFF;
        psp->backupboby();
        if (pp->WpnRocketType == 2 && !pp->NukeInitialized)
        {
            pp->TestNukeInit = false;
            pSetState(psp, ps_InitNuke);
        }
        else
            pSetState(psp, psp->RestState);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pMicroBobSetup(DPanelSprite* psp)
{
    if (psp->flags & (PANF_BOB))
        return;

    psp->backupbobcoords();

    psp->sin_amt = MICRO_BOB_X_AMT;
    psp->sin_ndx = 0;
    psp->bob_height_divider = 8;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pMicroHide(DPanelSprite* psp)
{
    psp->backupy();
    psp->pos.Y += 3 * synctics;

    if (psp->pos.Y >= MICRO_YOFF + pspheight(psp) + 20)
    {
        psp->opos.Y = psp->pos.Y = MICRO_YOFF + pspheight(psp) + 20;
        psp->opos.X = psp->pos.X = MICRO_XOFF;

        pWeaponUnHideKeys(psp, psp->PresentState);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

bool pMicroOverlays(DPanelSprite* psp)
{
#define MICRO_SIGHT_XOFF 29
#define MICRO_SIGHT_YOFF -58

#define MICRO_SHOT_XOFF 65
#define MICRO_SHOT_YOFF -41

#define MICRO_HEAT_XOFF 78
#define MICRO_HEAT_YOFF -51

    if (psp->over[MICRO_SIGHT_NUM].xoff == -1)
    {
        psp->over[MICRO_SIGHT_NUM].xoff = MICRO_SIGHT_XOFF;
        psp->over[MICRO_SIGHT_NUM].yoff = MICRO_SIGHT_YOFF;
        psp->over[MICRO_SHOT_NUM].xoff = MICRO_SHOT_XOFF;
        psp->over[MICRO_SHOT_NUM].yoff = MICRO_SHOT_YOFF;
        psp->over[MICRO_HEAT_NUM].xoff = MICRO_HEAT_XOFF;
        psp->over[MICRO_HEAT_NUM].yoff = MICRO_HEAT_YOFF;

    }

    if (psp->PlayerP->WpnRocketNuke == 0 && psp->PlayerP->WpnRocketType == 2)
        psp->PlayerP->WpnRocketType=0;

    switch (psp->PlayerP->WpnRocketType)
    {
    case 0:
        psp->over[MICRO_SIGHT_NUM].pic = MICRO_SIGHT;
        psp->over[MICRO_SHOT_NUM].pic = MICRO_SHOT_1;
        psp->over[MICRO_SHOT_NUM].flags |= (psf_ShadeNone);
        psp->over[MICRO_HEAT_NUM].pic = -1;
        return false;
    case 1:
        if (psp->PlayerP->WpnRocketHeat)
        {
            psp->over[MICRO_SIGHT_NUM].pic = MICRO_SIGHT;
            psp->over[MICRO_SHOT_NUM].pic = MICRO_SHOT_1;
            psp->over[MICRO_SHOT_NUM].flags |= (psf_ShadeNone);

            ASSERT(psp->PlayerP->WpnRocketHeat < 6);

            psp->over[MICRO_HEAT_NUM].pic = MICRO_HEAT + (5 - psp->PlayerP->WpnRocketHeat);
        }
        else
        {
            psp->over[MICRO_SIGHT_NUM].pic = MICRO_SIGHT;
            psp->over[MICRO_SHOT_NUM].pic = MICRO_SHOT_1;
            psp->over[MICRO_SHOT_NUM].flags |= (psf_ShadeNone);
            psp->over[MICRO_HEAT_NUM].pic = -1;
        }

        return false;
    case 2:
        psp->over[MICRO_SIGHT_NUM].pic = -1;
        psp->over[MICRO_HEAT_NUM].pic = -1;

        psp->over[MICRO_SHOT_NUM].pic = MICRO_SHOT_20;
        psp->over[MICRO_SHOT_NUM].flags |= (psf_ShadeNone);
        psp->over[MICRO_HEAT_NUM].flags |= (psf_ShadeNone);
        return true;
    }
    return false;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

PANEL_STATE ps_MicroHeatFlash[] =
{
    {MICRO_HEAT, 30, nullptr, &ps_MicroHeatFlash[1], 0,0,0},
    {0,             30, nullptr, &ps_MicroHeatFlash[2], 0,0,0},
    {MICRO_HEAT,    30, nullptr, &ps_MicroHeatFlash[3], 0,0,0},
    {0,             30, nullptr, &ps_MicroHeatFlash[4], 0,0,0},
    {MICRO_HEAT,    30, nullptr, &ps_MicroHeatFlash[5], 0,0,0},
    {0,             30, nullptr, &ps_MicroHeatFlash[6], 0,0,0},
    {MICRO_HEAT,    30, nullptr, &ps_MicroHeatFlash[7], 0,0,0},
    {0,             30, nullptr, &ps_MicroHeatFlash[8], 0,0,0},
    {MICRO_HEAT,    30, nullptr, &ps_MicroHeatFlash[9], 0,0,0},
    {0,              0, nullptr, nullptr, 0,0,0}
};

PANEL_STATE ps_MicroNukeFlash[] =
{
    {MICRO_SHOT_20, 30, nullptr, &ps_MicroNukeFlash[1], 0,0,0},
    {0,             30, nullptr, &ps_MicroNukeFlash[2], 0,0,0},
    {MICRO_SHOT_20, 30, nullptr, &ps_MicroNukeFlash[3], 0,0,0},
    {0,             30, nullptr, &ps_MicroNukeFlash[4], 0,0,0},
    {MICRO_SHOT_20, 30, nullptr, &ps_MicroNukeFlash[5], 0,0,0},
    {0,             30, nullptr, &ps_MicroNukeFlash[6], 0,0,0},
    {MICRO_SHOT_20, 30, nullptr, &ps_MicroNukeFlash[7], 0,0,0},
    {0,             30, nullptr, &ps_MicroNukeFlash[8], 0,0,0},
    {MICRO_SHOT_20, 30, nullptr, &ps_MicroNukeFlash[9], 0,0,0},
    {0,              0, nullptr, nullptr, 0,0,0}
};

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pMicroRest(DPanelSprite* psp)
{
    DSWPlayer* pp = psp->PlayerP;
    bool force = !!(psp->flags & PANF_UNHIDE_SHOOT);

    if (pWeaponHideKeys(psp, ps_MicroHide))
        return;

    pMicroOverlays(psp);

    pMicroBobSetup(psp);
    pWeaponBob(psp, PLAYER_MOVING(psp->PlayerP));

    if (pp->InitingNuke)
    {
        int choose_voc=0;

        pp->InitingNuke = false;
        pp->NukeInitialized = true;

        if (pp == getPlayer(myconnectindex))
        {
            choose_voc = StdRandomRange(1024);
            if (choose_voc > 600)
                PlayerSound(DIGI_TAUNTAI2,v3df_dontpan|v3df_follow,psp->PlayerP);
            else if (choose_voc > 300)
                PlayerSound(DIGI_TAUNTAI4,v3df_dontpan|v3df_follow,psp->PlayerP);
        }
    }

    if ((psp->PlayerP->cmd.ucmd.actions & SB_FIRE) || force)
    {
        if ((psp->PlayerP->KeyPressBits & SB_FIRE) || force)
        {
            psp->flags &= ~(PANF_UNHIDE_SHOOT);

            if (!WeaponOK(psp->PlayerP))
                return;

            if (psp->PlayerP->WpnAmmo[WPN_MICRO] <= 0 && psp->PlayerP->WpnRocketType != 2)
            {
                psp->PlayerP->WpnRocketNuke = 0;
                WeaponOK(psp->PlayerP);
                psp->PlayerP->WpnRocketNuke = 1;
                return;
            }

            switch (psp->PlayerP->WpnRocketType)
            {
            case 0:
            case 1:
                pSetState(psp, ps_MicroSingleFire);
                DoPlayerChooseYell(psp->PlayerP);
                break;
            case 2:
                if (psp->PlayerP->WpnRocketNuke > 0)
                    pSetState(psp, ps_MicroSingleFire);
                break;
            }
        }
    }
    else
        WeaponOK(psp->PlayerP);
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pMicroAction(DPanelSprite* psp)
{
    pMicroBobSetup(psp);
    pWeaponBob(psp, PLAYER_MOVING(psp->PlayerP));
}


//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pMicroFire(DPanelSprite* psp)
{
    SpawnVis(psp->PlayerP->GetActor(), nullptr, {}, 20);
    switch (psp->PlayerP->WpnRocketType)
    {
    case 0:
        if (sw_bunnyrockets)
            InitBunnyRocket(psp->PlayerP);
        else
            InitRocket(psp->PlayerP);
        break;
    case 1:
        if (sw_bunnyrockets)
            InitBunnyRocket(psp->PlayerP);
        else
            InitRocket(psp->PlayerP);
        break;
    case 2:
        PlaySound(DIGI_WARNING,psp->PlayerP,v3df_dontpan|v3df_follow);
        InitNuke(psp->PlayerP);
        psp->PlayerP->NukeInitialized = false;
        break;
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pMicroRetract(DPanelSprite* psp)
{
    psp->backupy();
    psp->pos.Y += 3 * synctics;

    if (psp->pos.Y >= MICRO_YOFF + pspheight(psp))
    {
        psp->PlayerP->Flags &= ~(PF_WEAPON_RETRACT);
        psp->PlayerP->Wpn[WPN_MICRO] = nullptr;
        pKillSprite(psp);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pNukeAction(DPanelSprite* psp)
{
    DSWPlayer* pp = psp->PlayerP;

#if 0 // Code commented out as it's causing interpolation issues when initialising a nuke.
    psp->backupy();
    psp->y -= 3 * synctics;

    if (psp->y < MICRO_YOFF)
    {
        psp->oy = psp->y = MICRO_YOFF;
        psp->backupboby();
    }
#endif

    pMicroBobSetup(psp);
    pWeaponBob(psp, PLAYER_MOVING(psp->PlayerP));
    if (!pp->InitingNuke)
        pSetState(psp, psp->PresentState);
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pMicroStandBy(DPanelSprite* psp)
{
    DSWPlayer* pp = psp->PlayerP;

    pMicroOverlays(psp);
    PlaySound(DIGI_NUKESTDBY, pp, v3df_follow|v3df_dontpan, CHAN_WEAPON);
}

void pMicroCount(DPanelSprite* psp)
{
    DSWPlayer* pp = psp->PlayerP;

    PlaySound(DIGI_NUKECDOWN, pp, v3df_follow|v3df_dontpan, CHAN_WEAPON);
}

void pMicroReady(DPanelSprite* psp)
{
    DSWPlayer* pp = psp->PlayerP;

    PlaySound(DIGI_NUKEREADY, pp, v3df_follow|v3df_dontpan, CHAN_WEAPON);
    pp->NukeInitialized = true;
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// HEART
//
//////////////////////////////////////////////////////////////////////////////////////////

void pHeartPresent(DPanelSprite* psp);
void pHeartRetract(DPanelSprite* psp);
void pHeartAction(DPanelSprite* psp);
void pHeartRest(DPanelSprite* psp);
void pHeartAttack(DPanelSprite* psp);
void pHeartHide(DPanelSprite* psp);
void pHeartActionBlood(DPanelSprite* psp);
void SpawnSmallHeartBlood(DPanelSprite* psp);

extern PANEL_STATE ps_HeartAttack[];
extern PANEL_STATE ps_ReloadHeart[];


#define Heart_BEAT_RATE 60
#define Heart_ACTION_RATE 10

PANEL_STATE ps_PresentHeart[] =
{
    {ID_HeartPresent0, Heart_BEAT_RATE, pHeartPresent, &ps_PresentHeart[1], 0,0,0},
    {ID_HeartPresent1, Heart_BEAT_RATE, pHeartPresent, &ps_PresentHeart[0], 0,0,0}
};

PANEL_STATE ps_HeartRest[] =
{
    {ID_HeartPresent0, Heart_BEAT_RATE, pHeartRest, &ps_HeartRest[1], 0,0,0},
    {ID_HeartPresent1, Heart_BEAT_RATE, pHeartRest, &ps_HeartRest[2], 0,0,0},
    {ID_HeartPresent1, Heart_BEAT_RATE, SpawnSmallHeartBlood, &ps_HeartRest[3], psf_QuickCall, 0,0},
    {ID_HeartPresent1, 0,               pHeartRest, &ps_HeartRest[0], 0,0,0},
};

PANEL_STATE ps_HeartHide[] =
{
    {ID_HeartPresent0, Heart_BEAT_RATE, pHeartHide, &ps_HeartHide[1], 0,0,0},
    {ID_HeartPresent1, Heart_BEAT_RATE, pHeartHide, &ps_HeartHide[0], 0,0,0}
};

PANEL_STATE ps_HeartAttack[] =
{
    // squeeze
    {ID_HeartAttack0, Heart_ACTION_RATE, pHeartActionBlood, &ps_HeartAttack[1], psf_ShadeHalf, 0,0},
    {ID_HeartAttack1, Heart_ACTION_RATE, pHeartActionBlood, &ps_HeartAttack[2], psf_ShadeNone, 0,0},
    {ID_HeartAttack1, Heart_ACTION_RATE, pHeartActionBlood, &ps_HeartAttack[3], psf_ShadeNone, 0,0},
    // attack
    {ID_HeartAttack1, Heart_ACTION_RATE, pHeartAttack, &ps_HeartAttack[4], psf_QuickCall, 0,0},
    // unsqueeze
    {ID_HeartAttack1, Heart_ACTION_RATE, pHeartAction, &ps_HeartAttack[5], psf_ShadeNone, 0,0},
    {ID_HeartAttack1, Heart_ACTION_RATE, pHeartAction, &ps_HeartAttack[6], psf_ShadeNone, 0,0},
    {ID_HeartAttack0, Heart_ACTION_RATE, pHeartAction, &ps_HeartAttack[7], psf_ShadeHalf, 0,0},

    {ID_HeartAttack0, Heart_ACTION_RATE, pHeartAction, &ps_HeartRest[0], psf_ShadeHalf, 0,0},
};

PANEL_STATE ps_RetractHeart[] =
{
    {ID_HeartPresent0, Heart_BEAT_RATE, pHeartRetract, &ps_RetractHeart[1], 0,0,0},
    {ID_HeartPresent1, Heart_BEAT_RATE, pHeartRetract, &ps_RetractHeart[0], 0,0,0}
};

#define HEART_YOFF 212

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void InitWeaponHeart(DSWPlayer* pp)
{
    DPanelSprite* psp;

    if (SW_SHAREWARE) return;

    if (Prediction)
        return;

    if (!(pp->WpnFlags &BIT(WPN_HEART)) ||
//        pp->WpnAmmo[WPN_HEART] <= 0 ||
        (pp->Flags & PF_WEAPON_RETRACT))
        return;

    if (!pp->Wpn[WPN_HEART])
    {
        psp = pp->Wpn[WPN_HEART] = pSpawnSprite(pp, ps_PresentHeart, PRI_MID, 160 + 10, HEART_YOFF);
        psp->pos.Y += pspheight(psp);
        psp->backupy();
    }

    if (pp->CurWpn == pp->Wpn[WPN_HEART])
    {
        return;
    }

    PlayerUpdateWeapon(pp, WPN_HEART);

    pp->WpnUziType = 2; // Make uzi's go away!
    RetractCurWpn(pp);

    PlaySound(DIGI_HEARTBEAT, pp, v3df_follow|v3df_dontpan|v3df_doppler);

    // Set up the new weapon variables
    psp = pp->CurWpn = pp->Wpn[WPN_HEART];
    psp->flags |= (PANF_WEAPON_SPRITE);
    psp->ActionState = ps_HeartAttack;
    psp->RetractState = ps_RetractHeart;
    psp->PresentState = ps_PresentHeart;
    psp->RestState = ps_HeartRest;
    pSetState(psp, psp->PresentState);

    psp->PlayerP->KeyPressBits |= SB_FIRE;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pHeartPresent(DPanelSprite* psp)
{
    if (psp->PlayerP->Flags & (PF_WEAPON_RETRACT))
        return;

    psp->backupy();
    psp->pos.Y -= 3 * synctics;

    if (psp->pos.Y < HEART_YOFF)
    {
        psp->opos.Y = psp->pos.Y = HEART_YOFF;
        psp->backupboby();
        pSetState(psp, psp->RestState);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pHeartBobSetup(DPanelSprite* psp)
{
    if (psp->flags & (PANF_BOB))
        return;

    psp->backupbobcoords();

    psp->sin_amt = 12;
    psp->sin_ndx = 0;
    psp->bob_height_divider = 8;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pHeartHide(DPanelSprite* psp)
{
    psp->backupy();
    psp->pos.Y += 3 * synctics;

    if (psp->pos.Y >= HEART_YOFF + pspheight(psp))
    {
        psp->opos.Y = psp->pos.Y = HEART_YOFF + pspheight(psp);

        pWeaponUnHideKeys(psp, psp->PresentState);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pHeartRest(DPanelSprite* psp)
{
    bool force = !!(psp->flags & PANF_UNHIDE_SHOOT);

    if (pWeaponHideKeys(psp, ps_HeartHide))
        return;

    psp->bobpos.Y += synctics;

    if (psp->bobpos.Y > HEART_YOFF)
    {
        psp->bobpos.Y = HEART_YOFF;
    }

    pHeartBobSetup(psp);
    pWeaponBob(psp, PLAYER_MOVING(psp->PlayerP));

    if ((psp->PlayerP->cmd.ucmd.actions & SB_FIRE) || force)
    {
        if ((psp->PlayerP->KeyPressBits & SB_FIRE) || force)
        {
            psp->PlayerP->KeyPressBits &= ~SB_FIRE;
            psp->flags &= ~(PANF_UNHIDE_SHOOT);

            if (!WeaponOK(psp->PlayerP))
                return;

            DoPlayerChooseYell(psp->PlayerP);
            pSetState(psp, psp->ActionState);
        }
    }
    else
    {
        psp->PlayerP->KeyPressBits |= SB_FIRE;
        WeaponOK(psp->PlayerP);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pHeartAction(DPanelSprite* psp)
{
    psp->bobpos.Y -= synctics;

    if (psp->bobpos.Y < 200)
    {
        psp->bobpos.Y = 200;
    }

    pHeartBobSetup(psp);
    pWeaponBob(psp, PLAYER_MOVING(psp->PlayerP));
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pHeartActionBlood(DPanelSprite* psp)
{
    psp->bobpos.Y -= synctics;

    if (psp->bobpos.Y < 200)
    {
        psp->bobpos.Y = 200;
    }

    psp->backupy();
    psp->pos.Y = psp->bobpos.Y;

    pHeartBobSetup(psp);
    pWeaponBob(psp, PLAYER_MOVING(psp->PlayerP));

    SpawnHeartBlood(psp);
}

void InitHeartAttack(DSWPlayer* pp);

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pHeartAttack(DPanelSprite* psp)
{
    DSWPlayer* pp = psp->PlayerP;
    // CTW MODIFICATION
    //int InitHeartAttack(PLAYER* pp);
    // CTW MODIFICATION END

    PlaySound(DIGI_HEARTFIRE, pp, v3df_follow|v3df_dontpan);
    if (RandomRange(1000) > 800)
        PlayerSound(DIGI_JG9009, v3df_follow|v3df_dontpan,pp);
    InitHeartAttack(psp->PlayerP);
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pHeartRetract(DPanelSprite* psp)
{
    psp->backupy();
    psp->pos.Y += 3 * synctics;

    if (psp->pos.Y >= HEART_YOFF + pspheight(psp))
    {
        psp->PlayerP->Flags &= ~(PF_WEAPON_RETRACT);
        psp->PlayerP->Wpn[WPN_HEART] = nullptr;
        pKillSprite(psp);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// HEART BLOOD
//
//////////////////////////////////////////////////////////////////////////////////////////

void pHeartBlood(DPanelSprite*);

#define HEART_BLOOD_RATE 10

PANEL_STATE ps_HeartBlood[] =
{
    {ID_HeartBlood0, HEART_BLOOD_RATE, pHeartBlood, &ps_HeartBlood[1], 0,0,0},
    {ID_HeartBlood1, HEART_BLOOD_RATE, pHeartBlood, &ps_HeartBlood[2], 0,0,0},
    {ID_HeartBlood2, HEART_BLOOD_RATE, pHeartBlood, &ps_HeartBlood[3], 0,0,0},
    {ID_HeartBlood3, HEART_BLOOD_RATE, pHeartBlood, &ps_HeartBlood[4], 0,0,0},
    {ID_HeartBlood4, HEART_BLOOD_RATE, pHeartBlood, &ps_HeartBlood[5], 0,0,0},
    {ID_HeartBlood5, HEART_BLOOD_RATE, pHeartBlood, &ps_HeartBlood[6], 0,0,0},
    {ID_HeartBlood5, HEART_BLOOD_RATE, pSuicide, &ps_HeartBlood[6], 0,0,0},
};

#define HEART_BLOOD_SMALL_RATE 7

PANEL_STATE ps_HeartBloodSmall[] =
{
    {ID_HeartBlood0, HEART_BLOOD_SMALL_RATE, pHeartBlood, &ps_HeartBlood[1], 0,0,0},
    {ID_HeartBlood1, HEART_BLOOD_SMALL_RATE, pHeartBlood, &ps_HeartBlood[2], 0,0,0},
    {ID_HeartBlood2, HEART_BLOOD_SMALL_RATE, pHeartBlood, &ps_HeartBlood[3], 0,0,0},
    {ID_HeartBlood3, HEART_BLOOD_SMALL_RATE, pHeartBlood, &ps_HeartBlood[4], 0,0,0},
    {ID_HeartBlood4, HEART_BLOOD_SMALL_RATE, pHeartBlood, &ps_HeartBlood[5], 0,0,0},
    {ID_HeartBlood5, HEART_BLOOD_SMALL_RATE, pHeartBlood, &ps_HeartBlood[6], 0,0,0},
    {ID_HeartBlood5, HEART_BLOOD_SMALL_RATE, pSuicide, &ps_HeartBlood[6], 0,0,0},
};

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void SpawnHeartBlood(DPanelSprite* psp)
{
    DSWPlayer* pp = psp->PlayerP;
    DPanelSprite* blood;
    PANEL_SHRAP* hsp;

    static PANEL_SHRAP HeartShrap[] =
    {
        {-10, -80, 2, -FIXED(1,32000), -FIXED(2,32000), -FIXED(5,32000), -FIXED(3,32000), {ps_HeartBlood, ps_HeartBlood}},
        {0, -85, 0, -FIXED(3,32000), -FIXED(8,32000), -FIXED(3,32000), -FIXED(1,32000), {ps_HeartBlood, ps_HeartBlood}},
        {10, -85, 2, -FIXED(1,32000), -FIXED(2,32000), FIXED(2,32000), FIXED(3,32000), {ps_HeartBlood, ps_HeartBlood}},
        {25, -80, 2, -FIXED(1,32000), -FIXED(2,32000), FIXED(5,32000), FIXED(6,32000), {ps_HeartBlood, ps_HeartBlood}},
        {0, 0, 0, 0, 0, 0, 0, {0, 0}},
    };

    for (hsp = HeartShrap; hsp->lo_jump_speed; hsp++)
    {
        if (hsp->skip == 2)
        {
            if (MoveSkip2 != 0)
                continue;
        }

        // RIGHT side
        blood = pSpawnSprite(pp, hsp->state[RANDOM_P2(2<<8)>>8], PRI_BACK, 0, 0);
        blood->pos.X = psp->pos.X + hsp->xoff;
        blood->opos.X = blood->pos.X;
        blood->pos.Y = psp->pos.Y + hsp->yoff;
        blood->opos.Y = blood->pos.Y;
        blood->xspeed = hsp->lo_xspeed + (RandomRange((hsp->hi_xspeed - hsp->lo_xspeed)>>4) << 4);
        blood->flags |= (PANF_WEAPON_SPRITE);

        blood->scale = 20000 + RandomRange(50000 - 20000);

        blood->jump_speed = hsp->lo_jump_speed + (RandomRange((hsp->hi_jump_speed + hsp->lo_jump_speed)>>4) << 4);
        DoBeginPanelJump(blood);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void SpawnSmallHeartBlood(DPanelSprite* psp)
{
    DSWPlayer* pp = psp->PlayerP;
    DPanelSprite* blood;
    PANEL_SHRAP* hsp;

    static PANEL_SHRAP HeartShrap[] =
    {
        {-10, -80, 0, -FIXED(1,0), -FIXED(2,0), -FIXED(1,0), -FIXED(3,0), {ps_HeartBloodSmall, ps_HeartBloodSmall}},
        {0, -85, 0, -FIXED(1,0), -FIXED(5,0), -FIXED(1,0), -FIXED(1,0), {ps_HeartBloodSmall, ps_HeartBloodSmall}},
        {10, -85, 0, -FIXED(1,0), -FIXED(2,0), FIXED(1,0), FIXED(2,0), {ps_HeartBloodSmall, ps_HeartBloodSmall}},
        {25, -80, 0, -FIXED(1,0), -FIXED(2,0), FIXED(3,0), FIXED(4,0), {ps_HeartBloodSmall, ps_HeartBloodSmall}},
        {0, 0, 0, 0, 0, 0, 0, {0,0}},
    };

    PlaySound(DIGI_HEARTBEAT, pp, v3df_follow|v3df_dontpan|v3df_doppler);

    for (hsp = HeartShrap; hsp->lo_jump_speed; hsp++)
    {
        // RIGHT side
        blood = pSpawnSprite(pp, hsp->state[RANDOM_P2(2<<8)>>8], PRI_BACK, 0, 0);
        blood->pos.X = psp->pos.X + hsp->xoff;
        blood->opos.X = blood->pos.X;
        blood->pos.Y = psp->pos.Y + hsp->yoff;
        blood->opos.Y = blood->pos.Y;
        blood->xspeed = hsp->lo_xspeed + (RandomRange((hsp->hi_xspeed - hsp->lo_xspeed)>>4) << 4);
        blood->flags |= (PANF_WEAPON_SPRITE);

        blood->scale = 10000 + RandomRange(30000 - 10000);

        blood->jump_speed = hsp->lo_jump_speed + (RandomRange((hsp->hi_jump_speed + hsp->lo_jump_speed)>>4) << 4);
        DoBeginPanelJump(blood);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pHeartBlood(DPanelSprite* psp)
{
    if (psp->flags & (PANF_JUMPING))
    {
        DoPanelJump(psp);
    }
    else if (psp->flags & (PANF_FALLING))
    {
        DoPanelFall(psp);
    }

    psp->backupx();
    psp->pos.X += psp->xspeed * (1. / FRACUNIT);

    if (psp->pos.X > 320 || psp->pos.X < 0 || psp->pos.Y > 200)
    {
        pKillSprite(psp);
        return;
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

int DoBeginPanelJump(DPanelSprite* psp)
{
#define PANEL_JUMP_GRAVITY FIXED(0,8000)

    psp->flags |= (PANF_JUMPING);
    psp->flags &= ~(PANF_FALLING);

    // set up individual actor jump gravity
    psp->jump_grav = PANEL_JUMP_GRAVITY;

    DoPanelJump(psp);

    return 0;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

int DoPanelJump(DPanelSprite* psp)
{
    // adjust jump speed by gravity - if jump speed greater than 0 player
    // have started falling
    if ((psp->jump_speed += psp->jump_grav) > 0)
    {
        // Start falling
        DoBeginPanelFall(psp);
        return 0;
    }

    // adjust height by jump speed
    psp->backupy();
    psp->pos.Y += psp->jump_speed * synctics * (1. / FRACUNIT);

    return 0;
}


//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

int DoBeginPanelFall(DPanelSprite* psp)
{
    psp->flags |= (PANF_FALLING);
    psp->flags &= ~(PANF_JUMPING);

    psp->jump_grav = PANEL_JUMP_GRAVITY;

    DoPanelFall(psp);

    return 0;
}


//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

int DoPanelFall(DPanelSprite* psp)
{
    // adjust jump speed by gravity
    psp->jump_speed += psp->jump_grav;

    // adjust player height by jump speed
    psp->backupy();
    psp->pos.Y += psp->jump_speed * synctics * (1. / FRACUNIT);

    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// GRENADE
//
//////////////////////////////////////////////////////////////////////////////////////////

void pGrenadePresent(DPanelSprite* psp);
void pGrenadeRetract(DPanelSprite* psp);
void pGrenadeAction(DPanelSprite* psp);
void pGrenadeRest(DPanelSprite* psp);
void pGrenadeFire(DPanelSprite* psp);
void pGrenadeHide(DPanelSprite* psp);
void pGrenadeUnHide(DPanelSprite* psp);
void pGrenadeRecoilDown(DPanelSprite* psp);
void pGrenadeRecoilUp(DPanelSprite* psp);
void pGrenadeBobSetup(DPanelSprite* psp);

extern PANEL_STATE ps_GrenadeRecoil[];

#define Grenade_REST_RATE 24
#define Grenade_ACTION_RATE 6

PANEL_STATE ps_PresentGrenade[] =
{
    {ID_GrenadePresent0, Grenade_REST_RATE, pGrenadePresent, &ps_PresentGrenade[0], 0,0,0}
};

PANEL_STATE ps_GrenadeRest[] =
{
    {ID_GrenadePresent0, Grenade_REST_RATE, pGrenadeRest, &ps_GrenadeRest[0], 0,0,0}
};

PANEL_STATE ps_GrenadeHide[] =
{
    {ID_GrenadePresent0, Grenade_REST_RATE, pGrenadeHide, &ps_GrenadeHide[0], 0,0,0}
};

PANEL_STATE ps_GrenadeFire[] =
{
    {ID_GrenadeFire0, Grenade_ACTION_RATE, pGrenadeAction, &ps_GrenadeFire[1], psf_ShadeHalf, 0,0},
    {ID_GrenadeFire1, Grenade_ACTION_RATE, pGrenadeAction, &ps_GrenadeFire[2], psf_ShadeNone, 0,0},
    {ID_GrenadeFire2, Grenade_ACTION_RATE, pGrenadeAction, &ps_GrenadeFire[3], psf_ShadeNone, 0,0},

    {ID_GrenadePresent0, 0, pGrenadeFire, &ps_GrenadeFire[4], psf_QuickCall, 0,0},
    {ID_GrenadePresent0, 3, pGrenadeAction, &ps_GrenadeRecoil[0], 0,0,0}
};

PANEL_STATE ps_GrenadeRecoil[] =
{
    // recoil
    {ID_GrenadePresent0, Grenade_REST_RATE, pGrenadeRecoilDown, &ps_GrenadeRecoil[0], 0,0,0},
    {ID_GrenadePresent0, Grenade_REST_RATE, pGrenadeRecoilUp, &ps_GrenadeRecoil[1], 0,0,0},
    // reload
    {ID_GrenadeReload0, Grenade_REST_RATE/2, pNullAnimator, &ps_GrenadeRecoil[3], 0,0,0},
    {ID_GrenadeReload1, Grenade_REST_RATE/2, pNullAnimator, &ps_GrenadeRecoil[4], 0,0,0},
    // ready to fire again
    {ID_GrenadePresent0, 3, pNullAnimator, &ps_GrenadeRest[0], 0,0,0}
};

PANEL_STATE ps_RetractGrenade[] =
{
    {ID_GrenadePresent0, Grenade_REST_RATE, pGrenadeRetract, &ps_RetractGrenade[0], 0,0,0}
};

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

#define GRENADE_YOFF 200
#define GRENADE_XOFF (160+20)

void pGrenadeSetRecoil(DPanelSprite* psp)
{
    psp->vel = 900;
    psp->ang = NORM_ANGLE(-256);
}

void pGrenadePresentSetup(DPanelSprite* psp)
{
    psp->rotate_ang = 1800;
    psp->backupcoords();
    psp->pos.Y += 34;
    psp->pos.X -= 45;
    psp->ang = 256 + 128;
    psp->vel = 680;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void InitWeaponGrenade(DSWPlayer* pp)
{
    DPanelSprite* psp;

    if (Prediction)
        return;

    if (!(pp->WpnFlags &BIT(WPN_GRENADE)) ||
//        pp->WpnAmmo[WPN_GRENADE] <= 0 ||
        (pp->Flags & PF_WEAPON_RETRACT))
        return;

    if (!pp->Wpn[WPN_GRENADE])
    {
        psp = pp->Wpn[WPN_GRENADE] = pSpawnSprite(pp, ps_PresentGrenade, PRI_MID, GRENADE_XOFF, GRENADE_YOFF);
        psp->pos.Y += pspheight(psp);
        psp->backupy();
    }

    if (pp->CurWpn == pp->Wpn[WPN_GRENADE])
    {
        return;
    }

    PlayerUpdateWeapon(pp, WPN_GRENADE);

    pp->WpnUziType = 2; // Make uzi's go away!
    RetractCurWpn(pp);

    // Set up the new weapon variables
    psp = pp->CurWpn = pp->Wpn[WPN_GRENADE];
    psp = pp->CurWpn = pp->Wpn[WPN_GRENADE];
    psp->flags |= (PANF_WEAPON_SPRITE);
    psp->ActionState = ps_GrenadeFire;
    psp->RetractState = ps_RetractGrenade;
    psp->PresentState = ps_PresentGrenade;
    psp->RestState = ps_GrenadeRest;
    pSetState(psp, psp->PresentState);

    pGrenadePresentSetup(psp);

    PlaySound(DIGI_GRENADE_UP, pp, v3df_follow);

    psp->PlayerP->KeyPressBits |= SB_FIRE;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pGrenadeRecoilDown(DPanelSprite* psp)
{
    psp->backupcoords();

    psp->pos.X += pspCosVel(psp);
    psp->pos.Y -= pspSinVel(psp);

    psp->vel -= 24 * synctics;

    if (psp->vel < 400)
    {
        psp->vel = 400;
        psp->ang = NORM_ANGLE(psp->ang + 1024);

        pStatePlusOne(psp);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pGrenadeRecoilUp(DPanelSprite* psp)
{
    psp->backupcoords();

    psp->pos.X += pspCosVel(psp);
    psp->pos.Y -= pspSinVel(psp);

    psp->vel += 15 * synctics;

    if (psp->pos.Y < GRENADE_YOFF)
    {
        psp->opos.Y = psp->pos.Y = GRENADE_YOFF;
        psp->opos.X = psp->pos.X = GRENADE_XOFF;

        pGrenadeSetRecoil(psp);

        pStatePlusOne(psp);
        psp->flags &= ~(PANF_BOB);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pGrenadePresent(DPanelSprite* psp)
{
    if (psp->PlayerP->Flags & (PF_WEAPON_RETRACT))
        return;

    psp->backupcoords();

    psp->pos.X += pspCosVel(psp);
    psp->pos.Y -= pspSinVel(psp);

    psp->rotate_ang = NORM_ANGLE(psp->rotate_ang + (6 * synctics));

    if (psp->rotate_ang < 1024)
        psp->rotate_ang = 0;

    if (psp->pos.Y < GRENADE_YOFF)
    {
        pGrenadeSetRecoil(psp);
        psp->opos.X = psp->pos.X = GRENADE_XOFF;
        psp->opos.Y = psp->pos.Y = GRENADE_YOFF;
        psp->rotate_ang = 0;
        psp->backupboby();
        pSetState(psp, psp->RestState);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pGrenadeBobSetup(DPanelSprite* psp)
{
    if (psp->flags & (PANF_BOB))
        return;

    psp->backupbobcoords();

    psp->sin_amt = 12;
    psp->sin_ndx = 0;
    psp->bob_height_divider = 8;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pGrenadeHide(DPanelSprite* psp)
{
    psp->backupy();
    psp->pos.Y += 3 * synctics;

    if (psp->pos.Y >= GRENADE_YOFF + pspheight(psp))
    {
        psp->opos.Y = psp->pos.Y = GRENADE_YOFF + pspheight(psp);
        psp->opos.X = psp->pos.X = GRENADE_XOFF;

        pGrenadePresentSetup(psp);

        pWeaponUnHideKeys(psp, psp->PresentState);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pGrenadeRest(DPanelSprite* psp)
{
    bool force = !!(psp->flags & PANF_UNHIDE_SHOOT);

    if (pWeaponHideKeys(psp, ps_GrenadeHide))
        return;

    pGrenadeBobSetup(psp);
    pWeaponBob(psp, PLAYER_MOVING(psp->PlayerP));

    if ((psp->PlayerP->cmd.ucmd.actions & SB_FIRE) || force)
    {
        if ((psp->PlayerP->KeyPressBits & SB_FIRE) || force)
        {
            psp->flags &= ~(PANF_UNHIDE_SHOOT);

            if (!WeaponOK(psp->PlayerP))
                return;

            DoPlayerChooseYell(psp->PlayerP);
            pSetState(psp, psp->ActionState);
        }
    }
    else
        WeaponOK(psp->PlayerP);
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pGrenadeAction(DPanelSprite* psp)
{
    pGrenadeBobSetup(psp);
    pWeaponBob(psp, PLAYER_MOVING(psp->PlayerP));
}


void pGrenadeFire(DPanelSprite* psp)
{
    SpawnVis(psp->PlayerP->GetActor(), nullptr, {}, 32);
    InitGrenade(psp->PlayerP);
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pGrenadeRetract(DPanelSprite* psp)
{
    psp->backupy();
    psp->pos.Y += 3 * synctics;

    if (psp->pos.Y >= GRENADE_YOFF + pspheight(psp))
    {
        psp->PlayerP->Flags &= ~(PF_WEAPON_RETRACT);
        psp->PlayerP->Wpn[WPN_GRENADE] = nullptr;
        pKillSprite(psp);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// MINE
//
//////////////////////////////////////////////////////////////////////////////////////////

void pMinePresent(DPanelSprite* psp);
void pMineRetract(DPanelSprite* psp);
void pMineAction(DPanelSprite* psp);
void pMineRest(DPanelSprite* psp);
void pMineThrow(DPanelSprite* psp);
void pMineLower(DPanelSprite* psp);
void pMineRaise(DPanelSprite* psp);
void pMineHide(DPanelSprite* psp);
void pMineUnHide(DPanelSprite* psp);
void pMineBobSetup(DPanelSprite* psp);
void pMineUpSound(DPanelSprite* psp);

#define Mine_REST_RATE 24
#define Mine_ACTION_RATE 6

PANEL_STATE ps_PresentMine[] =
{
    {ID_MinePresent0, Mine_REST_RATE, pMinePresent, &ps_PresentMine[0], 0,0,0}
};

PANEL_STATE ps_MineRest[] =
{
    {ID_MinePresent0, 36,               pMineRest,      &ps_MineRest[1], 0,0,0},
    {ID_MinePresent0, 0,                pMineUpSound,   &ps_MineRest[2], psf_QuickCall, 0,0},
    {ID_MinePresent1, Mine_REST_RATE,   pMineRest,      &ps_MineRest[2], 0,0,0},
};

PANEL_STATE ps_MineHide[] =
{
    {ID_MinePresent0, Mine_REST_RATE, pMineHide, &ps_MineHide[0], 0,0,0}
};

PANEL_STATE ps_MineThrow[] =
{
    {ID_MineThrow0,   3,                pNullAnimator,  &ps_MineThrow[1], 0,0,0},
    {ID_MineThrow0, Mine_ACTION_RATE, pMineThrow,       &ps_MineThrow[2],psf_QuickCall, 0,0},
    {ID_MineThrow0, Mine_ACTION_RATE, pMineLower,       &ps_MineThrow[2], 0,0,0},
    {ID_MineThrow0, Mine_ACTION_RATE*5, pNullAnimator,  &ps_MineThrow[4], 0,0,0},
    {ID_MinePresent0, Mine_ACTION_RATE, pMineRaise,     &ps_MineThrow[4], 0,0,0},
    {ID_MinePresent0, Mine_ACTION_RATE, pNullAnimator,  &ps_MineThrow[6], 0,0,0},
    {ID_MinePresent0, 3, pMineAction, &ps_MineRest[0], 0,0,0}
};

PANEL_STATE ps_RetractMine[] =
{
    {ID_MinePresent0, Mine_REST_RATE, pMineRetract, &ps_RetractMine[0], 0,0,0}
};

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

#define MINE_YOFF 200
//#define MINE_XOFF (160+20)
#define MINE_XOFF (160+50)

void InitWeaponMine(DSWPlayer* pp)
{
    DPanelSprite* psp;

    if (Prediction)
        return;

    if (pp->WpnAmmo[WPN_MINE] <= 0)
        PutStringInfo(pp,"Out of Sticky Bombs!");

    if (!(pp->WpnFlags &BIT(WPN_MINE)) ||
        pp->WpnAmmo[WPN_MINE] <= 0 ||
        (pp->Flags & PF_WEAPON_RETRACT))
        return;

    if (!pp->Wpn[WPN_MINE])
    {
        psp = pp->Wpn[WPN_MINE] = pSpawnSprite(pp, ps_PresentMine, PRI_MID, MINE_XOFF, MINE_YOFF);
        psp->pos.Y += pspheight(psp);
        psp->backupy();
    }

    if (pp->CurWpn == pp->Wpn[WPN_MINE])
    {
        return;
    }

    PlayerUpdateWeapon(pp, WPN_MINE);

    pp->WpnUziType = 2; // Make uzi's go away!
    RetractCurWpn(pp);

    // Set up the new weapon variables
    psp = pp->CurWpn = pp->Wpn[WPN_MINE];
    psp->flags |= (PANF_WEAPON_SPRITE);
    psp->ActionState = ps_MineThrow;
    psp->RetractState = ps_RetractMine;
    psp->PresentState = ps_PresentMine;
    psp->RestState = ps_MineRest;
    pSetState(psp, psp->PresentState);

    PlaySound(DIGI_PULL, pp, v3df_follow|v3df_dontpan);

    psp->PlayerP->KeyPressBits |= SB_FIRE;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pMineUpSound(DPanelSprite* psp)
{
    DSWPlayer* pp = psp->PlayerP;

    PlaySound(DIGI_MINE_UP, pp, v3df_follow);
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pMineLower(DPanelSprite* psp)
{
    psp->backupy();
    psp->pos.Y += 4 * synctics;

    if (psp->pos.Y > MINE_YOFF + pspheight(psp))
    {
        if (!WeaponOK(psp->PlayerP))
            return;
        psp->opos.Y = psp->pos.Y = MINE_YOFF + pspheight(psp);
        pStatePlusOne(psp);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pMineRaise(DPanelSprite* psp)
{
    psp->backupy();
    psp->pos.Y -= 4 * synctics;

    if (psp->pos.Y < MINE_YOFF)
    {
        psp->opos.Y = psp->pos.Y = MINE_YOFF;
        pStatePlusOne(psp);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pMinePresent(DPanelSprite* psp)
{
    if (psp->PlayerP->Flags & (PF_WEAPON_RETRACT))
        return;

    psp->backupy();
    psp->pos.Y -= 3 * synctics;

    if (psp->pos.Y < MINE_YOFF)
    {
        psp->opos.Y = psp->pos.Y = MINE_YOFF;
        psp->rotate_ang = 0;
        psp->backupboby();
        pSetState(psp, psp->RestState);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pMineBobSetup(DPanelSprite* psp)
{
    if (psp->flags & (PANF_BOB))
        return;

    psp->backupbobcoords();

    psp->sin_amt = 12;
    psp->sin_ndx = 0;
    psp->bob_height_divider = 8;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pMineHide(DPanelSprite* psp)
{
    psp->backupy();
    psp->pos.Y += 3 * synctics;

    if (psp->pos.Y >= MINE_YOFF + pspheight(psp))
    {
        psp->opos.Y = psp->pos.Y = MINE_YOFF + pspheight(psp);
        psp->opos.X = psp->pos.X = MINE_XOFF;

        pWeaponUnHideKeys(psp, psp->PresentState);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pMineRest(DPanelSprite* psp)
{
    bool force = !!(psp->flags & PANF_UNHIDE_SHOOT);

    if (pWeaponHideKeys(psp, ps_MineHide))
        return;

    pMineBobSetup(psp);
    pWeaponBob(psp, PLAYER_MOVING(psp->PlayerP));

    if ((psp->PlayerP->cmd.ucmd.actions & SB_FIRE) || force)
    {
        if ((psp->PlayerP->KeyPressBits & SB_FIRE) || force)
        {
            psp->flags &= ~(PANF_UNHIDE_SHOOT);

//            if (!WeaponOK(psp->PlayerP))
//                return;

            DoPlayerChooseYell(psp->PlayerP);
            pSetState(psp, psp->ActionState);
        }
    }
    else
        WeaponOK(psp->PlayerP);
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pMineAction(DPanelSprite* psp)
{
    pMineBobSetup(psp);
    pWeaponBob(psp, PLAYER_MOVING(psp->PlayerP));
}


//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pMineThrow(DPanelSprite* psp)
{
    InitMine(psp->PlayerP);
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pMineRetract(DPanelSprite* psp)
{
    psp->backupy();
    psp->pos.Y += 3 * synctics;

    if (psp->pos.Y >= MINE_YOFF + pspheight(psp))
    {
        psp->PlayerP->Flags &= ~(PF_WEAPON_RETRACT);
        psp->PlayerP->Wpn[WPN_MINE] = nullptr;
        pKillSprite(psp);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// CHOP STICKS
//
//////////////////////////////////////////////////////////////////////////////////////////

void pChopsUp(DPanelSprite* psp);
void pChopsDown(DPanelSprite* psp);
void pChopsDownSlow(DPanelSprite* psp);
void pChopsWait(DPanelSprite* psp);
void pChopsShake(DPanelSprite* psp);
void pChopsClick(DPanelSprite* psp);

void pChopsRetract(DPanelSprite* psp);

#define Chops_REST_RATE 24
#define Chops_ACTION_RATE 6

#define ID_ChopsRest  2000
#define ID_ChopsOpen  2001
#define ID_ChopsClose 2002

PANEL_STATE ps_ChopsAttack1[] =
{
    {ID_ChopsRest,  Chops_REST_RATE*3,  pNullAnimator,  &ps_ChopsAttack1[1], 0,0,0},
    {ID_ChopsRest,  Chops_REST_RATE,    pChopsUp,       &ps_ChopsAttack1[1], 0,0,0},
    {ID_ChopsOpen,  Chops_REST_RATE/3,  pNullAnimator,  &ps_ChopsAttack1[3], 0,0,0},
    {ID_ChopsClose, 0,                  pChopsClick,    &ps_ChopsAttack1[4], psf_QuickCall, 0,0},
    {ID_ChopsClose, Chops_REST_RATE/3,  pNullAnimator,  &ps_ChopsAttack1[5], 0,0,0},
    {ID_ChopsClose, Chops_REST_RATE,    pChopsDown,     &ps_ChopsAttack1[5], 0,0,0},
};

PANEL_STATE ps_ChopsAttack2[] =
{
    {ID_ChopsOpen,  Chops_REST_RATE*3,  pNullAnimator,  &ps_ChopsAttack2[1], 0,0,0},
    {ID_ChopsOpen,  Chops_REST_RATE,    pChopsUp,       &ps_ChopsAttack2[1], 0,0,0},
    {ID_ChopsOpen,  0,                  pChopsClick,    &ps_ChopsAttack2[3], psf_QuickCall, 0,0},
    {ID_ChopsOpen,  8,                  pNullAnimator,  &ps_ChopsAttack2[4], 0,0,0},
    {ID_ChopsRest,  Chops_REST_RATE,    pNullAnimator,  &ps_ChopsAttack2[5], 0,0,0},
    {ID_ChopsRest,  Chops_REST_RATE,    pChopsDown,     &ps_ChopsAttack2[5], 0,0,0},
};

PANEL_STATE ps_ChopsAttack3[] =
{
    {ID_ChopsOpen,  Chops_REST_RATE*3,  pNullAnimator,  &ps_ChopsAttack3[1], 0,0,0},
    {ID_ChopsOpen,  Chops_REST_RATE,    pChopsUp,       &ps_ChopsAttack3[1], 0,0,0},

    {ID_ChopsRest,  0,                  pNullAnimator,  &ps_ChopsAttack3[3], 0,0,0},
    {ID_ChopsRest,  0,                  pChopsClick,    &ps_ChopsAttack3[4], psf_QuickCall, 0,0},

    {ID_ChopsRest,  Chops_REST_RATE,    pNullAnimator,  &ps_ChopsAttack3[5], 0,0,0},
    {ID_ChopsRest,  24,                 pNullAnimator,  &ps_ChopsAttack3[6], 0,0,0},
    {ID_ChopsOpen,  16,                 pNullAnimator,  &ps_ChopsAttack3[7], 0,0,0},

    {ID_ChopsRest,  0,                  pChopsClick,    &ps_ChopsAttack3[8], psf_QuickCall, 0,0},
    {ID_ChopsRest,  16,                 pNullAnimator,  &ps_ChopsAttack3[9], 0,0,0},
    {ID_ChopsOpen,  16,                 pNullAnimator,  &ps_ChopsAttack3[10], 0,0,0},

    {ID_ChopsOpen,  8,                  pChopsDownSlow, &ps_ChopsAttack3[11], 0,0,0},
    {ID_ChopsRest,  10,                 pChopsDownSlow, &ps_ChopsAttack3[12], 0,0,0},
    {ID_ChopsRest,  0,                  pChopsClick,    &ps_ChopsAttack3[13], psf_QuickCall, 0,0},
    {ID_ChopsRest,  10,                 pChopsDownSlow, &ps_ChopsAttack3[14], 0,0,0},
    {ID_ChopsOpen,  10,                 pChopsDownSlow, &ps_ChopsAttack3[11], 0,0,0},
};

PANEL_STATE ps_ChopsAttack4[] =
{
    {ID_ChopsOpen,  Chops_REST_RATE*3,  pNullAnimator,  &ps_ChopsAttack4[1], 0,0,0},
    {ID_ChopsOpen,  Chops_REST_RATE,    pChopsUp,       &ps_ChopsAttack4[1], 0,0,0},

    {ID_ChopsOpen,  0,                  pChopsClick,    &ps_ChopsAttack4[3], psf_QuickCall, 0,0},
    {ID_ChopsOpen,  8,                  pNullAnimator,  &ps_ChopsAttack4[4], 0,0,0},

    {ID_ChopsRest,  Chops_REST_RATE,    pNullAnimator,  &ps_ChopsAttack4[5], 0,0,0},
    {ID_ChopsRest,  Chops_REST_RATE*4,  pChopsShake,    &ps_ChopsAttack4[6], 0,0,0},
    {ID_ChopsRest,  Chops_REST_RATE,    pChopsDown,     &ps_ChopsAttack4[6], 0,0,0},
};

PANEL_STATE* psp_ChopsAttack[] = {ps_ChopsAttack1, ps_ChopsAttack2, ps_ChopsAttack3, ps_ChopsAttack4};

PANEL_STATE ps_ChopsWait[] =
{
    {ID_ChopsRest,  Chops_REST_RATE, pChopsWait,     &ps_ChopsWait[0], 0,0,0},
};

PANEL_STATE ps_ChopsRetract[] =
{
    {ID_ChopsRest, Chops_REST_RATE, pChopsRetract, &ps_ChopsRetract[0], 0,0,0}
};

#define CHOPS_YOFF 200
#define CHOPS_XOFF (160+20)

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void InitChops(DSWPlayer* pp)
{
    DPanelSprite* psp;

    if (!pp->Chops)
    {
        psp = pp->Chops = pSpawnSprite(pp, ps_ChopsAttack1, PRI_MID, CHOPS_XOFF, CHOPS_YOFF);
        psp->pos.Y += pspheight(psp);
        psp->backupy();
    }

    if (Prediction)
        return;

    // Set up the new weapon variables
    psp = pp->Chops;

    psp->flags |= (PANF_WEAPON_SPRITE);
    psp->ActionState = ps_ChopsAttack1;
    psp->PresentState = ps_ChopsAttack1;
    psp->RetractState = ps_ChopsRetract;
    psp->RestState = ps_ChopsAttack1;

    PlaySound(DIGI_BUZZZ, psp->PlayerP,v3df_none);

    if (RandomRange(1000) > 750)
        PlayerSound(DIGI_MRFLY,v3df_follow|v3df_dontpan,psp->PlayerP);

}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pChopsClick(DPanelSprite* psp)
{
    int16_t rnd_rng;
    PlaySound(DIGI_CHOP_CLICK,psp->PlayerP,v3df_none);

    rnd_rng = RandomRange(1000);
    if (rnd_rng > 950)
        PlayerSound(DIGI_SEARCHWALL,v3df_follow|v3df_dontpan,psp->PlayerP);
    else if (rnd_rng > 900)
        PlayerSound(DIGI_EVADEFOREVER,v3df_follow|v3df_dontpan,psp->PlayerP);
    else if (rnd_rng > 800)
        PlayerSound(DIGI_SHISEISI,v3df_follow|v3df_dontpan,psp->PlayerP);
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pChopsUp(DPanelSprite* psp)
{
    psp->backupy();
    psp->pos.Y -= 3 * synctics;

    if (psp->pos.Y < CHOPS_YOFF)
    {
        psp->opos.Y = psp->pos.Y = CHOPS_YOFF;
        pStatePlusOne(psp);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pChopsDown(DPanelSprite* psp)
{
    psp->backupy();
    psp->pos.Y += 3 * synctics;

    if (psp->pos.Y > CHOPS_YOFF+110)
    {
        psp->opos.Y = psp->pos.Y = CHOPS_YOFF+110;
        pSetState(psp, ps_ChopsWait);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pChopsDownSlow(DPanelSprite* psp)
{
    psp->backupy();
    psp->pos.Y += 1 * synctics;

    if (psp->pos.Y > CHOPS_YOFF+110)
    {
        psp->opos.Y = psp->pos.Y = CHOPS_YOFF+110;
        pSetState(psp, ps_ChopsWait);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pChopsShake(DPanelSprite* psp)
{
    psp->backupcoords();

    psp->pos.X += (RANDOM_P2(4<<8) * (1. / 256.)) - 2;
    psp->pos.Y += (RANDOM_P2(4<<8) * (1. / 256.)) - 2;

    if (psp->pos.Y < CHOPS_YOFF)
    {
        psp->opos.Y = psp->pos.Y = CHOPS_YOFF;
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pChopsWait(DPanelSprite* psp)
{
    //if (!paused && RANDOM_P2(1024) < 10)
    if (RANDOM_P2(1024) < 10)
    {
        // random x position
        // do a random attack here
        psp->opos.X = psp->pos.X = CHOPS_XOFF + (RANDOM_P2(128) - 64);

        PlaySound(DIGI_BUZZZ,psp->PlayerP,v3df_none);
        pSetState(psp, psp_ChopsAttack[RandomRange(SIZ(psp_ChopsAttack))]);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void ChopsSetRetract(DSWPlayer* pp)
 {
    if (pp == nullptr || pp->Chops == nullptr)
        return;

    pSetState(pp->Chops, pp->Chops->RetractState);
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pChopsRetract(DPanelSprite* psp)
{
    psp->backupy();
    psp->pos.Y += 6 * synctics;

    if (psp->pos.Y >= CHOPS_YOFF + pspheight(psp))
    {
        if (RandomRange(1000) > 800)
            PlayerSound(DIGI_GETTINGSTIFF,v3df_follow|v3df_dontpan,psp->PlayerP);
        psp->PlayerP->Chops = nullptr;
        pKillSprite(psp);
    }
}


//////////////////////////////////////////////////////////////////////////////////////////
//
// FIST
//
// KungFu Move Numbers: (Used to calculate damages, etc....
// 0: Normal Punch
// 1: Palm Heel Strike
// 2: Kick
// 3: Block
// 4: FISTS OF FURY - (Flury of fast punches) DUCK, then BLOCK and ATTACK
// 5: CHI STRIKE - (Devastating Palm Heel)    DUCK 2 secs, then ATTACK
// 6: DEATH TOUCH - (Make enemy bleed and does alot of damage) JUMP, DUCK, BLOCK, ATTACK
// 7: FIREBALL - BLOCK 4 secs, then ATTACK
// 8: HAMMER KICK - Run forward while holding BLOCK, JUMP, ATTACK
// 9: CYCLONE KICK - (Spin 5 circles with leg out to hit multiples)
//       180 KEY, BLOCK, JUMP, ATTACK
// 10: DESPERATION - Finish Move (Cling to enemy like sticky bomb and beat the crap out of him)
//       Player is invincible until he unattaches
//       LOW HEALTH < 20, TOUCH ENEMY, BLOCK then ATTACK
// 11: SUPER NOVA - Become a human nuclear bomb!
//       SUPER SECRET MOVE
//       >150 Health + >50 Armor + Shadow Spell ACTIVE, DUCK + BLOCK 3 secs, JUMP, ATTACK
//       Player will die, so it's only useful if you get more than 1 frag in the deal.
// 12: TOTAL INVISIBILITY - Worse than shadow spell, you go 100% invisible for 10 secs!
//       SUPER SECRET MOVE
//       >180 Health + Shadow Spell ACTIVE, BLOCK 4 secs, JUMP, ATTACK
// 13: FREEZE - Turn all enemies in your view to ice for 10 secs
//       SUPER SECRET MOVE
//       100 Armor + Shadow Spell ACTIVE, BLOCK 5 secs, DUCK, ATTACK
//
//////////////////////////////////////////////////////////////////////////////////////////

static short FistAngTable[] =
{
    82,
    168,
    256+64
};

short FistAng = 0;

void FistBlur(DPanelSprite* psp)
{
    psp->kill_tics -= synctics;
    if (psp->kill_tics <= 0)
    {
        pKillSprite(psp);
        return;
    }
    else if (psp->kill_tics <= 6)
    {
        psp->flags |= (PANF_TRANS_FLIP);
    }

    psp->shade += 10;
    // change sprites priority
    REMOVE(psp);
    psp->priority--;
    InsertPanelSprite(psp->PlayerP, psp);
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void SpawnFistBlur(DPanelSprite* psp)
{
    if (cl_nomeleeblur) return;

    DPanelSprite* nsp;
    //PICITEMp pip;

    if (psp->PlayerP->FistAng > 200)
        return;

    nsp = pSpawnSprite(psp->PlayerP, nullptr, PRI_BACK, psp->pos.X, psp->pos.Y);

    nsp->flags |= (PANF_WEAPON_SPRITE);
    nsp->ang = psp->ang;
    nsp->vel = psp->vel;
    nsp->PanelSpriteFunc = FistBlur;
    nsp->kill_tics = 9;
    nsp->shade = psp->shade + 10;

    nsp->picndx = -1;
    nsp->picnum = psp->picndx;

    if ((psp->State->flags & psf_Xflip))
        nsp->flags |= (PANF_XFLIP);

    nsp->rotate_ang = psp->rotate_ang;
    nsp->scale = psp->scale;

    nsp->flags |= (PANF_TRANSLUCENT);
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pFistPresent(DPanelSprite* psp);
void pFistRetract(DPanelSprite* psp);
void pFistAction(DPanelSprite* psp);
void pFistRest(DPanelSprite* psp);
void pFistAttack(DPanelSprite* psp);
void pFistSlide(DPanelSprite* psp);
void pFistSlideDown(DPanelSprite* psp);
void pFistHide(DPanelSprite* psp);
void pFistUnHide(DPanelSprite* psp);

void pFistSlideR(DPanelSprite* psp);
void pFistSlideDownR(DPanelSprite* psp);
void pFistBlock(DPanelSprite* psp);

extern PANEL_STATE ps_FistSwing[];
extern PANEL_STATE ps_ReloadFist[];

#define Fist_BEAT_RATE 16
#define Fist_ACTION_RATE 5

PANEL_STATE ps_PresentFist[] =
{
    {ID_FistPresent0, Fist_BEAT_RATE, pFistPresent, &ps_PresentFist[0], 0,0,0}
};

PANEL_STATE ps_FistRest[] =
{
    {ID_FistPresent0, Fist_BEAT_RATE, pFistRest, &ps_FistRest[0], 0,0,0}
};

PANEL_STATE ps_FistHide[] =
{
    {ID_FistPresent0, Fist_BEAT_RATE, pFistHide, &ps_FistHide[0], 0,0,0}
};

PANEL_STATE ps_PresentFist2[] =
{
    {ID_Fist2Present0, Fist_BEAT_RATE, pFistPresent, &ps_PresentFist2[0], 0,0,0}
};

PANEL_STATE ps_Fist2Rest[] =
{
    {ID_Fist2Present0, Fist_BEAT_RATE, pFistRest, &ps_Fist2Rest[0], 0,0,0}
};

PANEL_STATE ps_Fist2Hide[] =
{
    {ID_Fist2Present0, Fist_BEAT_RATE, pFistHide, &ps_Fist2Hide[0], 0,0,0}
};


#define FIST_PAUSE_TICS 6
#define FIST_SLIDE_TICS 6
#define FIST_MID_SLIDE_TICS 16

PANEL_STATE ps_FistSwing[] =
{
    {ID_FistSwing0, FIST_PAUSE_TICS,                    pNullAnimator,      &ps_FistSwing[1], 0,0,0},
    {ID_FistSwing1, FIST_SLIDE_TICS, /* start slide */ pNullAnimator,        &ps_FistSwing[2], 0,0,0},
    {ID_FistSwing2, 0, /* damage */ pFistAttack,       &ps_FistSwing[3], psf_QuickCall, 0,0},
    {ID_FistSwing2, FIST_MID_SLIDE_TICS, /* mid slide */ pFistSlideDown,    &ps_FistSwing[4], 0,0,0},

    {ID_FistSwing2, 2, /* end slide */ pFistSlideDown,    &ps_FistSwing[4], 0,0,0},

    {ID_FistSwing1, FIST_SLIDE_TICS, /* start slide */ pFistSlideR,       &ps_FistSwing[6], psf_Xflip, 0,0},
    {ID_FistSwing2, 0, /* damage */ pFistAttack,       &ps_FistSwing[7], psf_QuickCall|psf_Xflip, 0,0},
    {ID_FistSwing2, FIST_MID_SLIDE_TICS, /* mid slide */ pFistSlideDownR,   &ps_FistSwing[8], psf_Xflip, 0,0},

    {ID_FistSwing2, 2, /* end slide */ pFistSlideDownR,   &ps_FistSwing[8], psf_Xflip, 0,0},
    {ID_FistSwing2, 2, /* end slide */ pNullAnimator,      &ps_FistSwing[1], psf_Xflip, 0,0},
};

PANEL_STATE ps_Fist2Swing[] =
{
    {4058, FIST_PAUSE_TICS,                    pNullAnimator,      &ps_Fist2Swing[1], 0,0,0},
    {4058, FIST_SLIDE_TICS, /* start slide */ pNullAnimator,      &ps_Fist2Swing[2], 0,0,0},
    {4058, 0, /* damage */ pFistBlock,         &ps_Fist2Swing[0], psf_QuickCall, 0,0},
    {4058, FIST_MID_SLIDE_TICS+5, /* mid slide */ pFistSlideDown,     &ps_Fist2Swing[4], 0,0,0},

    {4058, 2, /* end slide */ pFistSlideDown,    &ps_Fist2Swing[4], 0,0,0},
};

PANEL_STATE ps_Fist3Swing[] =
{
    {ID_Fist3Swing0, FIST_PAUSE_TICS+25,                    pNullAnimator,      &ps_Fist3Swing[1], 0,0,0},
    {ID_Fist3Swing1, 0, /* damage */ pFistAttack,       &ps_Fist3Swing[2], psf_QuickCall, 0,0},
    {ID_Fist3Swing2, FIST_PAUSE_TICS+10,                    pNullAnimator,      &ps_Fist3Swing[3], 0,0,0},
    {ID_Fist3Swing2, FIST_MID_SLIDE_TICS+3, /* mid slide */ pFistSlideDown,    &ps_Fist3Swing[4], 0,0,0},

    {ID_Fist3Swing2, 8, /* end slide */ pFistSlideDown,    &ps_Fist3Swing[4], 0,0,0},

    {ID_Fist3Swing1, FIST_SLIDE_TICS+20, /* start slide */ pFistSlideR,       &ps_Fist3Swing[6], psf_Xflip, 0,0},
    {ID_Fist3Swing2, 0, /* damage */ pFistAttack,       &ps_Fist3Swing[7], psf_QuickCall|psf_Xflip, 0,0},
    {ID_Fist3Swing2, FIST_MID_SLIDE_TICS+3, /* mid slide */ pFistSlideDownR,   &ps_Fist3Swing[8], psf_Xflip, 0,0},

    {ID_Fist3Swing2, 8, /* end slide */ pFistSlideDownR,   &ps_Fist3Swing[8], psf_Xflip, 0,0},
    {ID_Fist3Swing2, 8, /* end slide */ pNullAnimator,      &ps_Fist3Swing[1], psf_Xflip, 0,0},
};

#define KICK_PAUSE_TICS 40
#define KICK_SLIDE_TICS 30
#define KICK_MID_SLIDE_TICS 20

PANEL_STATE ps_Kick[] =
{
    {ID_Kick0, KICK_PAUSE_TICS,                    pNullAnimator,     &ps_Kick[1], 0,0,0},
    {ID_Kick1, 0, /* damage */ pFistAttack,       &ps_Kick[2], psf_QuickCall, 0,0},
    {ID_Kick1, KICK_SLIDE_TICS, /* start slide */ pNullAnimator,     &ps_Kick[3], 0,0,0},
    {ID_Kick1, KICK_MID_SLIDE_TICS, /* mid slide */ pFistSlideDown,    &ps_Kick[4], 0,0,0},

    {ID_Kick1, 30, /* end slide */ pFistSlideDown,    &ps_Kick[4], 0,0,0},

    {ID_Kick0, KICK_SLIDE_TICS, /* start slide */ pNullAnimator,     &ps_Kick[6], psf_Xflip, 0,0},
    {ID_Kick1, 0, /* damage */ pFistAttack,       &ps_Kick[7], psf_QuickCall|psf_Xflip, 0,0},
    {ID_Kick1, KICK_MID_SLIDE_TICS,/* mid slide */ pFistSlideDownR,   &ps_Kick[8], psf_Xflip, 0, 0},

    {ID_Kick1, 30, /* end slide */ pFistSlideDownR,   &ps_Kick[8], psf_Xflip, 0,0},
    {ID_Kick1, 30, /* end slide */ pNullAnimator,     &ps_Kick[1], psf_Xflip, 0,0},
};

PANEL_STATE ps_RetractFist[] =
{
    {ID_FistPresent0, Fist_BEAT_RATE, pFistRetract, &ps_RetractFist[0], 0,0,0}
};

#define FIST_SWAY_AMT 12

// left swing
#define FIST_XOFF (290 + FIST_SWAY_AMT)
#define FIST_YOFF 200

// right swing
#define FISTR_XOFF (0 - 80)

#define FIST_VEL 3000
#define FIST_POWER_VEL 3000

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void InitWeaponFist(DSWPlayer* pp)
{
    DPanelSprite* psp;
    short rnd_num;


    if (Prediction)
        return;

    if (!(pp->WpnFlags &BIT(WPN_FIST)) ||
        //pp->WpnAmmo[WPN_FIST] <= 0 ||
        (pp->Flags & PF_WEAPON_RETRACT))
    {
        pp->WpnFirstType = WPN_SWORD;
        InitWeaponSword(pp);
        return;
    }

    if (!pp->Wpn[WPN_FIST])
    {
        psp = pp->Wpn[WPN_FIST] = pSpawnSprite(pp, ps_PresentFist, PRI_MID, FIST_XOFF, FIST_YOFF);
        psp->pos.Y += pspheight(psp);
        psp->backupy();
    }

    if (pp->CurWpn == pp->Wpn[WPN_FIST])
    {
        return;
    }

    PlayerUpdateWeapon(pp, WPN_FIST);

    pp->WpnUziType = 2; // Make uzi's go away!
    RetractCurWpn(pp);

    // Set up the new weapon variables
    psp = pp->CurWpn = pp->Wpn[WPN_FIST];
    psp->flags |= (PANF_WEAPON_SPRITE);
    psp->ActionState = ps_FistSwing;
    psp->RetractState = ps_RetractFist;
    psp->PresentState = ps_PresentFist;
    psp->RestState = ps_FistRest;
    pSetState(psp, psp->PresentState);

    pp->WpnKungFuMove = 0; // Set to default strike

    rnd_num = RANDOM_P2(1024);
    if (rnd_num > 900)
        PlaySound(DIGI_TAUNTAI2, pp, v3df_follow|v3df_dontpan);
    else if (rnd_num > 800)
        PlaySound(DIGI_PLAYERYELL1, pp, v3df_follow|v3df_dontpan);
    else if (rnd_num > 700)
        PlaySound(DIGI_PLAYERYELL2, pp, v3df_follow|v3df_dontpan);

    psp->PlayerP->KeyPressBits |= SB_FIRE;
}


//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pFistPresent(DPanelSprite* psp)
{
    int rnd;

    if (psp->PlayerP->Flags & (PF_WEAPON_RETRACT))
        return;

    psp->backupy();
    psp->pos.Y -= 3 * synctics;

    if (psp->pos.Y < FIST_YOFF)
    {
        psp->opos.Y = psp->pos.Y = FIST_YOFF;
        psp->backupboby();

        rnd = RandomRange(1000);
        if (rnd > 500)
        {
            psp->PresentState = ps_PresentFist;
            psp->RestState = ps_FistRest;
        }
        else
        {
            psp->PresentState = ps_PresentFist2;
            psp->RestState = ps_Fist2Rest;
        }
        pSetState(psp, psp->RestState);
    }
}

//---------------------------------------------------------------------------
//
// LEFT SWING
//
//---------------------------------------------------------------------------

void pFistSlide(DPanelSprite* psp)
{
    SpawnFistBlur(psp);

    //psp->backupx();
    psp->backupy();

    //psp->x += pspSinVel(psp);
    psp->pos.Y -= pspSinVel(psp);

    psp->vel += 68 * synctics;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pFistSlideDown(DPanelSprite* psp)
{
    SpawnFistBlur(psp);

    short vel = 3500;
    auto ang = FistAng + psp->ang + psp->PlayerP->FistAng;

    psp->backupcoords();

    if (psp->ActionState == ps_Kick || psp->PlayerP->WpnKungFuMove == 3)
    {
        psp->pos.Y -= pspSinVel(psp, ang);
    }
    else
    {
        psp->pos.X -= pspSinVel(psp, ang);
        psp->pos.Y -= pspSinVel(psp, ang) * synctics;
    }

    psp->vel += 48 * synctics;

    if (psp->pos.Y > 440)
    {
        // if still holding down the fire key - continue swinging
        if (psp->PlayerP->cmd.ucmd.actions & SB_FIRE)
        {
            if (psp->PlayerP->KeyPressBits & SB_FIRE)
            {
                DoPlayerChooseYell(psp->PlayerP);

                if (RandomRange(1000) > 500)
                {
                    //if(RandomRange(1000) > 300)
                    //    {
                    psp->ActionState = ps_FistSwing;
                    psp->PlayerP->WpnKungFuMove = 0;
                    //    } else
                    //    {
                    //    psp->ActionState = ps_Fist3Swing;
                    //    psp->PlayerP->WpnKungFuMove = 1;
                    //    }
                    pSetState(psp, psp->ActionState);

                    psp->opos.X = psp->pos.X = FIST_XOFF;
                    psp->opos.Y = psp->pos.Y = FIST_YOFF;
                    psp->backupboby();
                    psp->PlayerP->FistAng = FistAngTable[RandomRange(SIZ(FistAngTable))];
                    psp->ang = 1024;
                    psp->vel = vel;
                    DoPlayerSpriteThrow(psp->PlayerP);
                    return;
                }
                else
                {
                    pSetState(psp, ps_FistSwing+(psp->State - psp->ActionState)+1);
                    psp->ActionState = ps_FistSwing;
                    psp->PlayerP->WpnKungFuMove = 0;
                }

                psp->opos.X = psp->pos.X = FISTR_XOFF+100;
                psp->opos.Y = psp->pos.Y = FIST_YOFF;
                psp->backupboby();
                psp->ang = 1024;
                psp->PlayerP->FistAng = FistAngTable[RandomRange(SIZ(FistAngTable))];
                psp->vel = vel;
                DoPlayerSpriteThrow(psp->PlayerP);
                return;
            }
        }

        // NOT still holding down the fire key - stop swinging
        pSetState(psp, psp->PresentState);
        psp->opos.X = psp->pos.X = FIST_XOFF;
        psp->opos.Y = psp->pos.Y = FIST_YOFF + pspheight(psp);
        psp->backupboby();
    }
}

//---------------------------------------------------------------------------
//
// RIGHT SWING
//
//---------------------------------------------------------------------------

void pFistSlideR(DPanelSprite* psp)
{
    SpawnFistBlur(psp);

    //psp->backupx();
    psp->backupy();

    //psp->x += pspSinVel(psp);
    psp->pos.Y += pspSinVel(psp);

    psp->vel += 68 * synctics;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pFistSlideDownR(DPanelSprite* psp)
{
    SpawnFistBlur(psp);

    short vel = 3500;
    auto ang = FistAng + psp->ang + psp->PlayerP->FistAng;

    psp->backupcoords();

    if (psp->ActionState == ps_Kick || psp->PlayerP->WpnKungFuMove == 3)
    {
        psp->pos.Y -= pspSinVel(psp, ang);
    }
    else
    {
        psp->pos.X -= pspSinVel(psp, ang);
        psp->pos.Y -= pspSinVel(psp, ang) * synctics;
    }

    psp->vel += 48 * synctics;

    if (psp->pos.Y > 440)
    {
        // if still holding down the fire key - continue swinging
        if (psp->PlayerP->cmd.ucmd.actions & SB_FIRE)
        {
            if (psp->PlayerP->KeyPressBits & SB_FIRE)
            {
                DoPlayerChooseYell(psp->PlayerP);

                if (RandomRange(1000) > 500)
                {
                    psp->ActionState = ps_FistSwing+5;
                    psp->PlayerP->WpnKungFuMove = 0;
                    pSetState(psp, psp->ActionState);

                    psp->opos.X = psp->pos.X = FISTR_XOFF+100;
                    psp->opos.Y = psp->pos.Y = FIST_YOFF;
                    psp->backupboby();
                    psp->ang = 1024;
                    psp->PlayerP->FistAng = FistAngTable[RandomRange(SIZ(FistAngTable))];
                    psp->vel = vel;
                    DoPlayerSpriteThrow(psp->PlayerP);
                    return;
                }
                else
                {
                    pSetState(psp, ps_FistSwing+(psp->State - psp->ActionState)+1);
                    psp->ActionState = ps_FistSwing;
                    psp->PlayerP->WpnKungFuMove = 0;
                }

                psp->opos.X = psp->pos.X = FIST_XOFF;
                psp->opos.Y = psp->pos.Y = FIST_YOFF;
                psp->backupboby();
                psp->PlayerP->FistAng = FistAngTable[RandomRange(SIZ(FistAngTable))];
                psp->ang = 1024;
                psp->vel = vel;
                DoPlayerSpriteThrow(psp->PlayerP);
                return;
            }
        }

        // NOT still holding down the fire key - stop swinging
        pSetState(psp, psp->PresentState);
        psp->opos.X = psp->pos.X = FIST_XOFF;
        psp->opos.Y = psp->pos.Y = FIST_YOFF + pspheight(psp);
        psp->backupboby();
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pFistBobSetup(DPanelSprite* psp)
{
    if (psp->flags & (PANF_BOB))
        return;

    psp->backupbobcoords();

    psp->sin_amt = FIST_SWAY_AMT;
    psp->sin_ndx = 0;
    psp->bob_height_divider = 8;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pFistHide(DPanelSprite* psp)
{
    psp->backupy();
    psp->pos.Y += 3 * synctics;

    if (psp->pos.Y >= FIST_YOFF + pspheight(psp))
    {
        psp->opos.Y = psp->pos.Y = FIST_YOFF + pspheight(psp);

        pWeaponUnHideKeys(psp, psp->PresentState);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pFistRest(DPanelSprite* psp)
{
    bool force = !!(psp->flags & PANF_UNHIDE_SHOOT);

    if (pWeaponHideKeys(psp, ps_FistHide))
        return;

    psp->bobpos.Y += synctics;

    if (psp->bobpos.Y > FIST_YOFF)
    {
        psp->bobpos.Y = FIST_YOFF;
    }

    pFistBobSetup(psp);
    pWeaponBob(psp, PLAYER_MOVING(psp->PlayerP));

    force = !!(psp->flags & PANF_UNHIDE_SHOOT);

    if (psp->ActionState == ps_Kick)
        psp->ActionState = ps_FistSwing;

    // Reset move to default
    psp->PlayerP->WpnKungFuMove = 0;

    if ((psp->PlayerP->cmd.ucmd.actions & SB_FIRE) || force)
    {
        if ((psp->PlayerP->KeyPressBits & SB_FIRE) || force)
        {
            psp->flags &= ~(PANF_UNHIDE_SHOOT);

            psp->ActionState = ps_FistSwing;
            psp->PlayerP->WpnKungFuMove = 0;

            pSetState(psp, psp->ActionState);

            psp->vel = 5500;

            psp->ang = 1024;
            psp->PlayerP->FistAng = FistAngTable[RandomRange(SIZ(FistAngTable))];
            DoPlayerSpriteThrow(psp->PlayerP);
        }
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pFistAction(DPanelSprite* psp)
{
    pFistBobSetup(psp);
    pWeaponBob(psp, PLAYER_MOVING(psp->PlayerP));
}


void pFistAttack(DPanelSprite* psp)
{
    InitFistAttack(psp->PlayerP);
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pFistRetract(DPanelSprite* psp)
{
    psp->backupy();
    psp->pos.Y += 3 * synctics;

    if (psp->pos.Y >= FIST_YOFF + pspheight(psp))
    {
        psp->PlayerP->Flags &= ~(PF_WEAPON_RETRACT);
        psp->PlayerP->Wpn[WPN_FIST] = nullptr;
        pKillSprite(psp);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pFistBlock(DPanelSprite* psp)
{
    psp->bobpos.Y += synctics;

    if (psp->bobpos.Y > FIST_YOFF)
    {
        psp->bobpos.Y = FIST_YOFF;
    }

    pFistBobSetup(psp);
    pWeaponBob(psp, PLAYER_MOVING(psp->PlayerP));

    if (!(psp->PlayerP->cmd.ucmd.actions & SB_OPEN))
    {
        pStatePlusOne(psp);
    }
}


//////////////////////////////////////////////////////////////////////////////////////////
//
// PANEL SPRITE GENERAL ROUTINES
//
//////////////////////////////////////////////////////////////////////////////////////////



void pWeaponForceRest(DSWPlayer* pp)
{
    pSetState(pp->CurWpn, pp->CurWpn->RestState);
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

bool pWeaponUnHideKeys(DPanelSprite* psp, PANEL_STATE* state)
{
    // initing the other weapon will take care of this
    if (psp->flags & (PANF_DEATH_HIDE))
    {
        return false;
    }

    if (psp->flags & (PANF_WEAPON_HIDE))
    {
        if (!(psp->PlayerP->Flags & PF_WEAPON_DOWN))
        {
            psp->flags &= ~(PANF_WEAPON_HIDE);
            pSetState(psp, state);
            return true;
        }

        return false;
    }

    if (psp->PlayerP->cmd.ucmd.actions & SB_HOLSTER)
    {
        if (psp->PlayerP->KeyPressBits & SB_HOLSTER)
        {
            psp->PlayerP->KeyPressBits &= ~SB_HOLSTER;
            pSetState(psp, state);
            return true;
        }
    }
    else
    {
        psp->PlayerP->KeyPressBits |= SB_HOLSTER;
    }

    if (psp->PlayerP->cmd.ucmd.actions & SB_FIRE)
    {
        if (psp->PlayerP->KeyPressBits & SB_FIRE)
        {
            psp->flags |= (PANF_UNHIDE_SHOOT);
            pSetState(psp, state);
            return true;
        }
    }

    return false;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

bool pWeaponHideKeys(DPanelSprite* psp, PANEL_STATE* state)
{
    if (psp->PlayerP->Flags & (PF_DEAD))
    {
        psp->flags |= (PANF_DEATH_HIDE);
        pSetState(psp, state);
        return true;
    }

    if (psp->PlayerP->Flags & (PF_WEAPON_DOWN))
    {
        psp->flags |= (PANF_WEAPON_HIDE);
        pSetState(psp, state);
        return true;
    }

    if (psp->PlayerP->cmd.ucmd.actions & SB_HOLSTER)
    {
        if (psp->PlayerP->KeyPressBits & SB_HOLSTER)
        {
            psp->PlayerP->KeyPressBits &= ~SB_HOLSTER;
            PutStringInfo(psp->PlayerP,"Weapon Holstered");
            pSetState(psp, state);
            return true;
        }
    }
    else
    {
        psp->PlayerP->KeyPressBits |= SB_HOLSTER;
    }

    return false;

}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void InsertPanelSprite(DSWPlayer* pp, DPanelSprite* psp)
{
    DPanelSprite* cur,* nxt;

    ASSERT(psp);

    // if list is empty, insert at front
    if (EMPTY(pp->PanelSpriteList))
    {
        INSERT(pp->PanelSpriteList, psp);
        return;
    }

    // if new pri is <= first pri in list, insert at front
    if (psp->priority <= pp->PanelSpriteList->Next->priority)
    {
        INSERT(pp->PanelSpriteList, psp);
        return;
    }

    // search for first pri in list thats less than the new pri
    auto list = pp->GetPanelSpriteList();
    for (cur = list->Next; nxt = cur->Next, cur != list; cur = nxt)
    {
        // if the next pointer is the end of the list, insert it
        if (cur->Next == pp->PanelSpriteList)
        {
            INSERT(cur, psp);
            return;
        }


        // if between cur and next, insert here
        if (psp->priority >= cur->priority && psp->priority <= cur->Next->priority)
        {
            INSERT(cur, psp);
            return;
        }
    }
}


//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

DPanelSprite* pSpawnSprite(DSWPlayer* pp, PANEL_STATE* state, uint8_t priority, double x, double y)
{
    unsigned i;
    DPanelSprite* psp;


    ASSERT(pp);

    psp = Create<DPanelSprite>();

    PRODUCTION_ASSERT(psp);

    psp->priority = priority;
    InsertPanelSprite(pp, psp);
    // INSERT(&pp->PanelSpriteList, psp);

    psp->PlayerP = pp;

    psp->opos.X = psp->pos.X = x;
    psp->opos.Y = psp->pos.Y = y;
    pSetState(psp, state);
    if (state == nullptr)
        psp->picndx = -1;
    else
        psp->picndx = state->picndx;
    psp->ang = 0;
    psp->vel = 0;
    psp->rotate_ang = 0;
    psp->scale = FRACUNIT;
    psp->ID = 0;

    for (i = 0; i < SIZ(psp->over); i++)
    {
        psp->over[i].State = nullptr;
        psp->over[i].pic = -1;
        psp->over[i].xoff = -1;
        psp->over[i].yoff = -1;
    }

    return psp;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pSuicide(DPanelSprite* psp)
{
    pKillSprite(psp);
}

void pKillSprite(DPanelSprite* psp)
{
    PRODUCTION_ASSERT(psp);

    REMOVE(psp);

    psp->Destroy();
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pClearSpriteList(DSWPlayer* pp)
{
    DPanelSprite* psp = nullptr, * next_psp = nullptr;
    auto l = pp->PanelSpriteList;

    for (psp = l->Next; next_psp = psp->Next, psp != l; psp = next_psp)
    {
        if (psp->Next == nullptr || psp->Prev == nullptr) break; // this can happen when cleaning up a fatal error.
        pKillSprite(psp);
    }
    assert(l == l->Prev  && l->Next == l);
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pWeaponBob(DPanelSprite* psp, short condition)
{
    double xdiff = 0, ydiff = 0;
	double bobvel = min(psp->PlayerP->vect.Length() * 10, 128.);

    if (condition)
    {
        psp->flags |= (PANF_BOB);
    }
    else
    {
        if (abs((psp->sin_ndx & 1023) - 0) < 70)
        {
            psp->flags &= ~(PANF_BOB);
            psp->sin_ndx = (RANDOM_P2(1024) < 512) ? 1024 : 0;
        }
    }

    if (cl_weaponsway && (psp->flags & PANF_BOB))
    {
        // //
        // sin_xxx moves the weapon left-right
        // //

        // increment the ndx into the sin table
        psp->sin_ndx = (psp->sin_ndx + (synctics << 3) + (cl_swsmoothsway ? (synctics << 2) : RANDOM_P2(8) * synctics)) & 2047;

        // get height
        xdiff = psp->sin_amt * BobVal(psp->sin_ndx);

        // //
        // bob_xxx moves the weapon up-down
        // //

        // as the weapon moves left-right move the weapon up-down in the same
        // proportion
        double bob_ndx = (psp->sin_ndx + 512) & 1023;

        // base bob amt on the players velocity - Max of 128
        double bobamt = bobvel / psp->bob_height_divider;
        ydiff = bobamt * BobVal(bob_ndx);
    }

    // Back up current coordinates for interpolating.
    psp->backupcoords();

    psp->pos.X = psp->bobpos.X + xdiff;
    psp->pos.Y = psp->bobpos.Y + ydiff + UziRecoilYadj;
}


//////////////////////////////////////////////////////////////////////////////////////////
//
// PANEL SPRITE CONTROL ROUTINES
//
//////////////////////////////////////////////////////////////////////////////////////////

bool DrawBeforeView = false;
void pDisplaySprites(DSWPlayer* pp, double interpfrac)
{
    DSWActor* plActor = pp->GetActor();
    DPanelSprite* next=nullptr;
    short shade, picnum, overlay_shade = 0;
    double x, y;
    unsigned i;

    uint8_t pal = 0;
    int flags;

    const auto offpair = pp->getWeaponOffsets(interpfrac);
    const auto offsets = offpair.first;

    auto list = pp->GetPanelSpriteList();
    for (auto psp = list->Next; next = psp->Next, psp != list; psp = next)
    {
        double ang = (offpair.second - DAngle::fromBuild(psp->rotate_ang)).Degrees();
        shade = 0;
        flags = 0;
        if (cl_hudinterpolation)
        {
            x = interpolatedvalue<double>(psp->opos.X, psp->pos.X, interpfrac);
            y = interpolatedvalue<double>(psp->opos.Y, psp->pos.Y, interpfrac);

        }
        else
        {
            x = psp->pos.X;
            y = psp->pos.Y;
        }

        x += offsets.X;
        y += offsets.Y;

        // initilize pal here - jack with it below
        pal = uint8_t(psp->pal);

        if (DrawBeforeView)
            if (!(psp->flags & PANF_DRAW_BEFORE_VIEW))
                continue;

        if (psp->flags & (PANF_SUICIDE))
        {
            //pKillSprite(psp);
            continue;
        }

        // if the state is null get the texture for other than picndx
        if (psp->picndx == -1 || !psp->State)
            picnum = psp->picnum;
        else
            picnum = psp->picndx;

        // UK panzies have to have darts instead of shurikens.
        if (sw_darts)
            switch (picnum)
            {
            case STAR_REST:
                picnum = 2510;
                break;
            case STAR_REST + 1:
                picnum = 2511;
                break;
            case STAR_REST + 2:
                picnum = 2512;
                break;
            case STAR_REST + 3:
                picnum = 2513;
                break;
            case STAR_REST + 4:
                picnum = 2514;
                break;
            case STAR_REST + 5:
                picnum = 2515;
                break;
            case STAR_REST + 6:
                picnum = 2516;
                break;
            case STAR_REST + 7:
                picnum = 2517;
                break;
            }

        if (pp->Bloody)
        {
            switch (picnum)
            {
            case SWORD_REST:
                picnum = BLOODYSWORD_REST;
                break;
            case SWORD_SWING0:
                picnum = BLOODYSWORD_SWING0;
                break;
            case SWORD_SWING1:
                picnum = BLOODYSWORD_SWING1;
                break;
            case SWORD_SWING2:
                picnum = BLOODYSWORD_SWING2;
                break;

            case FIST_REST:
                picnum = 4077;
                break;
            case FIST2_REST:
                picnum = 4051;
                break;
            case FIST_SWING0:
                picnum = 4074;
                break;
            case FIST_SWING1:
                picnum = 4075;
                break;
            case FIST_SWING2:
                picnum = 4076;
                break;

            case STAR_REST:
            case 2510:
                if (!sw_darts)
                    picnum = 2138;
                else
                    picnum = 2518; // Bloody Dart Hand
                break;
            }
        }

        if (pp->WpnShotgunType == 1)
        {
            switch (picnum)
            {
            case SHOTGUN_REST:
            case SHOTGUN_RELOAD0:
                picnum = 2227;
                break;
            case SHOTGUN_RELOAD1:
                picnum = 2226;
                break;
            case SHOTGUN_RELOAD2:
                picnum = 2225;
                break;
            }
        }

        // don't draw
        if (psp->flags & (PANF_INVISIBLE))
            continue;

        if (psp->State && (psp->State->flags & psf_Invisible))
            continue;

        // if its a weapon sprite and the view is set to the outside don't draw the sprite
        if (psp->flags & (PANF_WEAPON_SPRITE))
        {
            sectortype* sectp = nullptr;
            int16_t floorshade = 0;
            if (pp->insector())
            {
                sectp = pp->cursector;
                pal = sectp->floorpal;
                floorshade = sectp->floorshade;

                if (pal != PALETTE_DEFAULT)
                {
                    if (sectp->hasU() && (sectp->flags & SECTFU_DONT_COPY_PALETTE))
                        pal = PALETTE_DEFAULT;
                }

                if (pal == PALETTE_FOG || pal == PALETTE_DIVE || pal == PALETTE_DIVE_LAVA)
                    pal = uint8_t(psp->pal); // Set it back
            }

            ///////////

            if (pp->InventoryActive[INVENTORY_CLOAK])
            {
                flags |= (RS_TRANS1);
            }

            shade = overlay_shade = floorshade - 10;

            if (psp->PlayerP->Flags & (PF_VIEW_FROM_OUTSIDE))
            {
                if (!(psp->PlayerP->Flags & PF_VIEW_OUTSIDE_WEAPON))
                    continue;
            }

            if (psp->PlayerP->Flags & (PF_VIEW_FROM_CAMERA))
                continue;

            // !FRANK - this was moved from BELOW this IF statement
            // if it doesn't have a picflag or its in the view
            if (sectp && sectp->hasU() && (sectp->flags & SECTFU_DONT_COPY_PALETTE))
                pal = 0;
        }

        if (psp->flags & (PANF_TRANSLUCENT))
            flags |= (RS_TRANS1);

        flags |= (psp->flags & (PANF_TRANS_FLIP));

        if (psp->flags & (PANF_CORNER))
            flags |= (RS_TOPLEFT);

        if ((psp->State && (psp->State->flags & psf_Xflip)) || (psp->flags & PANF_XFLIP))
        {
            flags |= (RS_XFLIPHUD);
        }

        // shading
        if (psp->State && (psp->State->flags & (psf_ShadeHalf|psf_ShadeNone)))
        {
            if ((psp->State->flags & psf_ShadeNone))
                shade = 0;
            else if ((psp->State->flags & psf_ShadeHalf))
                shade /= 2;
        }

        if (pal == PALETTE_DEFAULT)
        {
            switch (picnum)
            {
            case 4080:
            case 4081:
            case 2220:
            case 2221:
                pal = plActor->user.spal;
                break;
            }
        }

        // temporary hack to fix fist artifacts until a solution is found in the panel system itself
        switch (picnum)
        {
            case FIST_SWING0:
            case FIST_SWING1:
            case FIST_SWING2:
            case FIST2_SWING0:
            case FIST2_SWING1:
            case FIST2_SWING2:
            case FIST3_SWING0:
            case FIST3_SWING1:
            case FIST3_SWING2:
            case BLOODYFIST_SWING0:
            case BLOODYFIST_SWING1:
            case BLOODYFIST_SWING2:
            case BLOODYFIST2_SWING0:
            case BLOODYFIST2_SWING1:
            case BLOODYFIST2_SWING2:
            case BLOODYFIST3_SWING0:
            case BLOODYFIST3_SWING1:
            case BLOODYFIST3_SWING2:
                if ((flags & RS_XFLIPHUD) && x > 160)
                    x = 65;
                else if (!(flags & RS_XFLIPHUD) && x < 160)
                    x = 345;
                break;
            default:
                break;
        }

		hud_drawsprite(x, y, psp->scale / 65536., ang, tileGetTextureID(picnum), shade, pal, flags);

        // do overlays (if any)
        for (i = 0; i < SIZ(psp->over); i++)
        {
            // get pic from state
            if (psp->over[i].State)
                picnum = psp->over[i].State->picndx;
            else
            // get pic from over variable
            if (psp->over[i].pic >= 0)
                picnum = psp->over[i].pic;
            else
                continue;

            if ((psp->over[i].flags & psf_ShadeNone))
                overlay_shade = 0;

            if (picnum)
            {
                hud_drawsprite((x + psp->over[i].xoff), (y + psp->over[i].yoff), psp->scale / 65536., ang, tileGetTextureID(picnum), overlay_shade, pal, flags);
            }
        }
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pSpriteControl(DSWPlayer* pp)
{
    DPanelSprite* next=nullptr;

    auto list = pp->GetPanelSpriteList();
    for (auto psp = list->Next; next = psp->Next, psp != list; psp = next)
    {
        // reminder - if these give an assertion look for pKillSprites
        // somewhere else other than by themselves
        // RULE: Sprites can only kill themselves
        PRODUCTION_ASSERT(psp);

        if (psp->State)
            pStateControl(psp);
        else
        // for sprits that are not state controled but still need to call
        // something
        if (psp->PanelSpriteFunc)
        {
            (*psp->PanelSpriteFunc)(psp);
        }
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pSetState(DPanelSprite* psp, PANEL_STATE* panel_state)
{
    PRODUCTION_ASSERT(psp);
    psp->tics = 0;
    psp->State = panel_state;
    psp->picndx = panel_state ? panel_state->picndx : 0;
}


void pNextState(DPanelSprite* psp)
{
    // Transition to the next state
    psp->State = psp->State->NextState;

    if ((psp->State->flags & psf_QuickCall))
    {
        (*psp->State->Animator)(psp);
        psp->State = psp->State->NextState;
    }
}

void pStatePlusOne(DPanelSprite* psp)
{
    psp->tics = 0;
    psp->State++;

    if ((psp->State->flags & psf_QuickCall))
    {
        (*psp->State->Animator)(psp);
        psp->State = psp->State->NextState;
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void pStateControl(DPanelSprite* psp)
{
    unsigned i;
    short tics = synctics;

    psp->tics += tics;

    // Skip states if too much time has passed
    while (psp->tics >= psp->State->tics)
    {
        // Set Tics
        psp->tics -= psp->State->tics;

        pNextState(psp);
    }

    // Set spritenum to the correct pic
    psp->picndx = psp->State->picndx;

    // do overlay states
    for (i = 0; i < SIZ(psp->over); i++)
    {
        if (!psp->over[i].State)
            continue;

        psp->over[i].tics += tics;

        // Skip states if too much time has passed
        while (psp->over[i].tics >= psp->over[i].State->tics)
        {
            // Set Tics
            psp->over[i].tics -= psp->over[i].State->tics;
            psp->over[i].State = psp->over[i].State->NextState;

            if (!psp->over[i].State)
                break;
        }
    }

    // Call the correct animator
    if (psp->State->Animator)
        (*psp->State->Animator)(psp);

}


//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void UpdatePanel(double interpfrac)
{
    short pnum;

    TRAVERSE_CONNECT(pnum)
    {
        if (pnum == screenpeek)
            pDisplaySprites(getPlayer(pnum), interpfrac);
    }
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void PreUpdatePanel(double interpfrac)
{
    short pnum;
    DrawBeforeView = true;

    //if (DrawBeforeView)
    TRAVERSE_CONNECT(pnum)
    {
        if (pnum == screenpeek)
            pDisplaySprites(getPlayer(pnum), interpfrac);
    }

    DrawBeforeView = false;
}

#define EnvironSuit_RATE 10
#define Fly_RATE 10
#define Cloak_RATE 10
#define Night_RATE 10
#define Box_RATE 10
#define Medkit_RATE 10
#define RepairKit_RATE 10
#define ChemBomb_RATE 10
#define FlashBomb_RATE 10
#define SmokeBomb_RATE 10
#define Caltrops_RATE 10

#define ID_PanelEnvironSuit 2397
PANEL_STATE ps_PanelEnvironSuit[] =
{
    {ID_PanelEnvironSuit, EnvironSuit_RATE, PanelInvTestSuicide, &ps_PanelEnvironSuit[0], 0,0,0}
};

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

#include "saveable.h"

static saveable_code saveable_panel_code[] =
{
    SAVE_CODE(pSuicide),
    SAVE_CODE(SwordBlur),
    SAVE_CODE(SpecialUziRetractFunc),
    SAVE_CODE(FistBlur),
};

static saveable_data saveable_panel_data[] =
{
    SAVE_DATA(ps_PresentSword),
    SAVE_DATA(ps_SwordRest),
    SAVE_DATA(ps_SwordHide),
    SAVE_DATA(ps_SwordSwing),
    SAVE_DATA(ps_RetractSword),

    SAVE_DATA(ps_PresentStar),
    SAVE_DATA(ps_StarHide),
    SAVE_DATA(ps_StarRest),
    SAVE_DATA(ps_ThrowStar),
    SAVE_DATA(ps_RetractStar),

    SAVE_DATA(ps_FireUzi),
    SAVE_DATA(ps_UziNull),
    SAVE_DATA(ps_UziHide),
    SAVE_DATA(ps_PresentUzi),
    SAVE_DATA(ps_PresentUziReload),
    SAVE_DATA(ps_RetractUzi),
    SAVE_DATA(ps_FireUzi2),
    SAVE_DATA(ps_PresentUzi2),
    SAVE_DATA(ps_Uzi2Hide),
    SAVE_DATA(ps_RetractUzi2),
    SAVE_DATA(ps_Uzi2Suicide),
    SAVE_DATA(ps_Uzi2Null),
    SAVE_DATA(ps_UziEject),
    SAVE_DATA(ps_UziClip),
    SAVE_DATA(ps_UziReload),
    SAVE_DATA(ps_UziDoneReload),
    SAVE_DATA(ps_UziShell),
    SAVE_DATA(ps_Uzi2Shell),

    SAVE_DATA(ps_ShotgunShell),
    SAVE_DATA(ps_PresentShotgun),
    SAVE_DATA(ps_ShotgunRest),
    SAVE_DATA(ps_ShotgunHide),
    SAVE_DATA(ps_ShotgunRecoil),
    SAVE_DATA(ps_ShotgunRecoilAuto),
    SAVE_DATA(ps_ShotgunFire),
    SAVE_DATA(ps_ShotgunAutoFire),
    SAVE_DATA(ps_ShotgunReload),
    SAVE_DATA(ps_RetractShotgun),
    SAVE_DATA(ps_ShotgunFlash),

    SAVE_DATA(ps_PresentRail),
    SAVE_DATA(ps_RailRest),
    SAVE_DATA(ps_RailHide),
    SAVE_DATA(ps_RailRecoil),
    SAVE_DATA(ps_RailFire),
    SAVE_DATA(ps_RailFireEMP),
    SAVE_DATA(ps_RetractRail),

    SAVE_DATA(ps_PresentHothead),
    SAVE_DATA(ps_HotheadHide),
    SAVE_DATA(ps_RetractHothead),
    SAVE_DATA(ps_HotheadRest),
    SAVE_DATA(ps_HotheadRestRing),
    SAVE_DATA(ps_HotheadRestNapalm),
    SAVE_DATA(ps_HotheadAttack),
    SAVE_DATA(ps_HotheadRing),
    SAVE_DATA(ps_HotheadNapalm),
    SAVE_DATA(ps_HotheadTurn),
    SAVE_DATA(ps_HotheadTurnRing),
    SAVE_DATA(ps_HotheadTurnNapalm),
    SAVE_DATA(ps_OnFire),

    SAVE_DATA(ps_PresentMicro),
    SAVE_DATA(ps_MicroRest),
    SAVE_DATA(ps_MicroHide),
    SAVE_DATA(ps_InitNuke),
    SAVE_DATA(ps_MicroRecoil),
    SAVE_DATA(ps_MicroFire),
    SAVE_DATA(ps_MicroSingleFire),
    SAVE_DATA(ps_RetractMicro),
    SAVE_DATA(ps_MicroHeatFlash),
    SAVE_DATA(ps_MicroNukeFlash),

    SAVE_DATA(ps_PresentHeart),
    SAVE_DATA(ps_HeartRest),
    SAVE_DATA(ps_HeartHide),
    SAVE_DATA(ps_HeartAttack),
    SAVE_DATA(ps_RetractHeart),
    SAVE_DATA(ps_HeartBlood),
    SAVE_DATA(ps_HeartBloodSmall),
    SAVE_DATA(ps_HeartBlood),

    SAVE_DATA(ps_PresentGrenade),
    SAVE_DATA(ps_GrenadeRest),
    SAVE_DATA(ps_GrenadeHide),
    SAVE_DATA(ps_GrenadeFire),
    SAVE_DATA(ps_GrenadeRecoil),
    SAVE_DATA(ps_RetractGrenade),

    SAVE_DATA(ps_PresentMine),
    SAVE_DATA(ps_MineRest),
    SAVE_DATA(ps_MineHide),
    SAVE_DATA(ps_MineThrow),
    SAVE_DATA(ps_RetractMine),

    SAVE_DATA(ps_ChopsAttack1),
    SAVE_DATA(ps_ChopsAttack2),
    SAVE_DATA(ps_ChopsAttack3),
    SAVE_DATA(ps_ChopsAttack4),
    SAVE_DATA(ps_ChopsWait),
    SAVE_DATA(ps_ChopsRetract),

    SAVE_DATA(ps_PresentFist),
    SAVE_DATA(ps_FistRest),
    SAVE_DATA(ps_FistHide),
    SAVE_DATA(ps_PresentFist2),
    SAVE_DATA(ps_Fist2Rest),
    SAVE_DATA(ps_Fist2Hide),
    /*
    SAVE_DATA(ps_PresentFist3),
    SAVE_DATA(ps_Fist3Rest),
    SAVE_DATA(ps_Fist3Hide),
    */
    SAVE_DATA(ps_FistSwing),
    SAVE_DATA(ps_Fist2Swing),
    SAVE_DATA(ps_Fist3Swing),
    SAVE_DATA(ps_Kick),
    SAVE_DATA(ps_RetractFist),

    SAVE_DATA(ps_PanelEnvironSuit),
};

saveable_module saveable_panel =
{
    // code
    saveable_panel_code,
    SIZ(saveable_panel_code),

    // data
    saveable_panel_data,
    SIZ(saveable_panel_data)
};

END_SW_NS
