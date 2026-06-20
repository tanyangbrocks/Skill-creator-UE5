#include "TotemLibrary.h"

using ESET = ESkillElementType;

const TArray<FTotemData>& FTotemLibrary::AllTotems()
{
    static const TArray<FTotemData> Data = []
    {
        TArray<FTotemData> T;
        auto Add = [&](const TCHAR* Id, const TCHAR* Name, ETotemType Type, int32 Cost, int32 ReqLevel = 0)
        {
            FTotemData D;
            D.TotemId = Id;
            D.DisplayName = FText::FromString(Name);
            D.Type = Type;
            D.BaseAbilityPointCost = Cost;
            D.RequiredPlayerLevel = ReqLevel;
            T.Add(MoveTemp(D));
        };

        // ── 範圍（無門檻）── Godot TotemLibrary.cs:19-22
        Add(TEXT("area_fan"),     TEXT("扇形衝擊"),     ETotemType::Area, 6);
        Add(TEXT("area_around"),  TEXT("周身衝擊"),     ETotemType::Area, 8);
        Add(TEXT("area_distant"), TEXT("遠距圓形衝擊"), ETotemType::Area, 10);
        Add(TEXT("area_beam"),    TEXT("射線衝擊"),     ETotemType::Area, 8);
        // ── 武技（無門檻）── Godot TotemLibrary.cs:26-28
        Add(TEXT("technique_sword"),  TEXT("劍技"), ETotemType::Technique, 8);
        Add(TEXT("technique_punch"),  TEXT("拳擊"), ETotemType::Technique, 6);
        Add(TEXT("technique_shield"), TEXT("盾防"), ETotemType::Technique, 7);
        // ── 投射物（無門檻）── Godot TotemLibrary.cs:32-33
        Add(TEXT("projectile_energy"),   TEXT("能量投射"), ETotemType::Projectile, 10);
        Add(TEXT("projectile_physical"), TEXT("實物投射"), ETotemType::Projectile, 12);
        // ── 被動（無門檻）── Godot TotemLibrary.cs:37-38
        Add(TEXT("passive_continuous"), TEXT("持續偵測"), ETotemType::Passive, 3);
        Add(TEXT("passive_switch"),     TEXT("開關式"),   ETotemType::Passive, 4);
        // ── 變幻（LV 20+）── Godot TotemLibrary.cs:41-45
        Add(TEXT("morph_speed"),      TEXT("加速形態"), ETotemType::Morph, 10, 20);
        Add(TEXT("morph_flight"),     TEXT("飛行形態"), ETotemType::Morph, 15, 20);
        Add(TEXT("morph_invisible"),  TEXT("隱身形態"), ETotemType::Morph, 15, 20);
        Add(TEXT("morph_strengthen"), TEXT("強化形態"), ETotemType::Morph, 12, 20);
        Add(TEXT("morph_possession"), TEXT("附體形態"), ETotemType::Morph, 20, 20);
        // ── 位移（LV 20+）── Godot TotemLibrary.cs:48-51
        Add(TEXT("displace_dash"),     TEXT("衝刺"),     ETotemType::Displacement, 8,  20);
        Add(TEXT("displace_teleport"), TEXT("瞬移"),     ETotemType::Displacement, 15, 20);
        Add(TEXT("displace_dodge"),    TEXT("閃避翻滾"), ETotemType::Displacement, 10, 20);
        Add(TEXT("displace_portal"),   TEXT("傳送門"),   ETotemType::Displacement, 18, 20);
        // ── 召喚（LV 30+）── Godot TotemLibrary.cs:54-59
        Add(TEXT("summon_minion"),   TEXT("召喚精靈"), ETotemType::Summon, 15, 30);
        Add(TEXT("summon_turret"),   TEXT("召喚砲台"), ETotemType::Summon, 18, 30);
        Add(TEXT("summon_guardian"), TEXT("召喚護衛"), ETotemType::Summon, 14, 30);
        Add(TEXT("summon_weapon"),   TEXT("召喚武器"), ETotemType::Summon, 16, 30);
        Add(TEXT("summon_building"), TEXT("召喚建築"), ETotemType::Summon, 22, 30);
        Add(TEXT("summon_vehicle"),  TEXT("召喚載具"), ETotemType::Summon, 20, 30);
        // ── 領域（LV 50+）── Godot TotemLibrary.cs:62-64
        Add(TEXT("domain_barrier"), TEXT("結界"),     ETotemType::Domain, 20, 50);
        Add(TEXT("domain_terrain"), TEXT("地形改造"), ETotemType::Domain, 25, 50);
        Add(TEXT("domain_weather"), TEXT("天候操控"), ETotemType::Domain, 30, 50);
        return T;
    }();
    return Data;
}

const TArray<FEngraveData>& FTotemLibrary::AllEngravings()
{
    static const TArray<FEngraveData> Data = []
    {
        TArray<FEngraveData> E;
        auto Add = [&](const TCHAR* Id, const TCHAR* Name, EEngraveColor Color,
                       EScalingType Scaling, float Coef, float BaseEffect, int32 BaseCost,
                       bool bRestriction = false, int32 ReqLevel = 0,
                       ESkillElementType Elem = ESET::None,
                       EEngraveCategory Category = EEngraveCategory::Modifier,
                       EEngraveTrigger Trigger = EEngraveTrigger::OnCast)
        {
            FEngraveData D;
            D.EngraveId     = Id;
            D.DisplayName   = FText::FromString(Name);
            D.Color         = Color;
            D.Scaling       = Scaling;
            D.ScalingCoefficient = Coef;
            D.BaseEffect    = BaseEffect;
            D.BaseCost      = BaseCost;
            D.bIsRestriction = bRestriction;
            D.RequiredPlayerLevel = ReqLevel;
            D.Element       = Elem;
            D.Category      = Category;
            D.Trigger       = Trigger;
            E.Add(MoveTemp(D));
        };

        using SC = EScalingType;
        using GC = EEngraveColor;

        // ── 白：傷害型 ── Godot TotemLibrary.cs:74-80
        Add(TEXT("white_dmg"),        TEXT("傷害增幅"), GC::White, SC::Hyperbolic, 1.0f, 0.f, 2);
        Add(TEXT("white_pen"),        TEXT("穿透"),     GC::White, SC::Hyperbolic, 0.5f, 0.f, 2);
        Add(TEXT("white_fixed"),      TEXT("固定傷害"), GC::White, SC::Linear,     5.f,  0.f, 2);
        Add(TEXT("white_crit"),       TEXT("暴擊機率"), GC::White, SC::Hyperbolic, 1.5f, 0.f, 3);
        Add(TEXT("white_multi_hit"),  TEXT("連續傷害"), GC::White, SC::Linear,     1.f,  1.f, 4);
        Add(TEXT("white_ignore_def"), TEXT("無視防禦"), GC::White, SC::Hyperbolic, 0.8f, 0.f, 5);
        Add(TEXT("white_ignore_res"), TEXT("無視抗性"), GC::White, SC::Hyperbolic, 0.8f, 0.f, 5);

        // ── 橙：控制型 ── Godot TotemLibrary.cs:83-92
        Add(TEXT("orange_push"),        TEXT("擊退"),     GC::Orange, SC::Linear,     1.f, 1.f, 3);
        Add(TEXT("orange_slow"),        TEXT("減速"),     GC::Orange, SC::Hyperbolic, 1.0f, 0.f, 3);
        Add(TEXT("orange_freeze"),      TEXT("凍結機率"), GC::Orange, SC::Hyperbolic, 2.0f, 0.f, 4);
        Add(TEXT("orange_pull"),        TEXT("牽引"),     GC::Orange, SC::Linear,     1.f, 0.f, 3);
        Add(TEXT("orange_levitate"),    TEXT("浮力"),     GC::Orange, SC::Linear,     1.f, 0.f, 4);
        Add(TEXT("orange_pos_swap"),    TEXT("位置交換"), GC::Orange, SC::Linear,     0.f, 1.f, 6);
        Add(TEXT("orange_rand_tp"),     TEXT("隨機傳送"), GC::Orange, SC::Linear,     1.f, 0.f, 5);
        Add(TEXT("orange_prop_change"), TEXT("性質改造"), GC::Orange, SC::Linear,     0.f, 1.f, 7);
        Add(TEXT("orange_reflect_atk"), TEXT("反射攻擊"), GC::Orange, SC::Hyperbolic, 1.0f, 0.f, 6);
        Add(TEXT("orange_charm"),       TEXT("操控"),     GC::Orange, SC::Hyperbolic, 0.5f, 0.f, 8);

        // ── 藍：技能因子改造 ── Godot TotemLibrary.cs:95-103
        // （blue_passive/blue_switch_mode 已於 Godot 移至被動技能因子層，不在此列）
        Add(TEXT("blue_multi"),             TEXT("多段發動"), GC::Blue, SC::Linear, 1.f,   1.f, 5);
        Add(TEXT("blue_chargeable"),        TEXT("可儲存式"), GC::Blue, SC::Linear, 0.5f,  1.f, 5);
        Add(TEXT("blue_condition_trigger"), TEXT("條件觸發"), GC::Blue, SC::Linear, 0.f,   1.f, 4);
        Add(TEXT("blue_no_interrupt"),      TEXT("不可打斷"), GC::Blue, SC::Linear, 0.f,   1.f, 4);
        Add(TEXT("blue_quick_cancel"),      TEXT("快速取消"), GC::Blue, SC::Linear, 0.1f,  0.f, 2);
        Add(TEXT("blue_track_trail"),       TEXT("軌跡記錄"), GC::Blue, SC::Linear, 0.5f,  1.f, 4);
        Add(TEXT("blue_track_replay"),      TEXT("軌跡回播"), GC::Blue, SC::Linear, 0.5f,  1.f, 6);

        // ── 紅：侵略效果 ── Godot TotemLibrary.cs:106-121
        Add(TEXT("red_stun"),           TEXT("暈眩機率"),     GC::Red, SC::Hyperbolic, 2.0f, 0.f, 4);
        Add(TEXT("red_instakill"),      TEXT("瞬殺機率"),     GC::Red, SC::Hyperbolic, 0.1f, 0.f, 8);
        Add(TEXT("red_combo_break"),    TEXT("斷招"),         GC::Red, SC::Hyperbolic, 1.5f, 0.f, 4);
        Add(TEXT("red_sense_deprive"),  TEXT("感官剝奪"),     GC::Red, SC::Hyperbolic, 1.0f, 0.f, 5);
        Add(TEXT("red_stack_amplify"),  TEXT("狀態疊加強化"), GC::Red, SC::Linear,     5.f,  0.f, 3);
        Add(TEXT("red_effect_steal"),   TEXT("效果劫奪"),     GC::Red, SC::Hyperbolic, 1.0f, 0.f, 6);
        Add(TEXT("red_target_replace"), TEXT("目標替代"),     GC::Red, SC::Hyperbolic, 1.0f, 0.f, 7);
        Add(TEXT("red_dot"),            TEXT("持續傷害"),     GC::Red, SC::Linear,     2.f,  5.f, 3);
        Add(TEXT("red_ability_seal"),   TEXT("能力封印"),     GC::Red, SC::Hyperbolic, 1.0f, 0.f, 7);
        Add(TEXT("red_ability_copy"),   TEXT("能力複製"),     GC::Red, SC::Hyperbolic, 0.5f, 0.f, 8);
        Add(TEXT("red_ability_steal"),  TEXT("能力奪取"),     GC::Red, SC::Hyperbolic, 0.3f, 0.f, 10);
        Add(TEXT("red_burn"),           TEXT("燒傷"),         GC::Red, SC::Linear,     1.f,  0.f, 3);
        Add(TEXT("red_bleed"),          TEXT("流血"),         GC::Red, SC::Linear,     1.f,  0.f, 3);
        Add(TEXT("red_petrify"),        TEXT("石化"),         GC::Red, SC::Hyperbolic, 1.5f, 0.f, 6);
        Add(TEXT("red_fear"),           TEXT("恐懼"),         GC::Red, SC::Hyperbolic, 1.5f, 0.f, 5);
        Add(TEXT("red_paralyze"),       TEXT("麻痺"),         GC::Red, SC::Hyperbolic, 1.5f, 0.f, 5);

        // ── 綠：輔助效果 ── Godot TotemLibrary.cs:124-135
        Add(TEXT("green_shield"),        TEXT("護盾值"),   GC::Green, SC::Linear,     10.f, 50.f, 3);
        Add(TEXT("green_heal"),          TEXT("回復效果"), GC::Green, SC::Linear,     5.f,  20.f, 2);
        Add(TEXT("green_death_replace"), TEXT("死亡替代"), GC::Green, SC::Hyperbolic, 0.5f, 0.f,  8);
        Add(TEXT("green_dmg_to_heal"),   TEXT("傷害轉治療"), GC::Green, SC::Hyperbolic, 1.0f, 0.f, 6);
        Add(TEXT("green_observe"),       TEXT("觀測強化"), GC::Green, SC::Linear,     1.f,  0.f,  2);
        Add(TEXT("green_evasion"),       TEXT("閃避強化"), GC::Green, SC::Hyperbolic, 1.0f, 0.f,  4);
        Add(TEXT("green_lifesteal"),     TEXT("吸血"),     GC::Green, SC::Hyperbolic, 1.0f, 0.f,  5);
        Add(TEXT("green_reflect"),       TEXT("反彈"),     GC::Green, SC::Hyperbolic, 1.0f, 0.f,  6);
        Add(TEXT("green_invincible"),    TEXT("無敵"),     GC::Green, SC::Linear,     0.f,  1.f,  10);
        Add(TEXT("green_super_armor"),   TEXT("霸體"),     GC::Green, SC::Linear,     0.f,  1.f,  7);
        Add(TEXT("green_tracking"),      TEXT("追蹤"),     GC::Green, SC::Hyperbolic, 0.8f, 0.f,  5);
        Add(TEXT("green_mark"),          TEXT("附加標記"), GC::Green, SC::Linear,     0.f,  1.f,  3);

        // ── 紫：額外操作 ── Godot TotemLibrary.cs:138-142
        Add(TEXT("purple_discover"),    TEXT("選擇型效果"),     GC::Purple, SC::Linear, 0.f,  2.f, 5);
        Add(TEXT("purple_rhythm"),      TEXT("節奏輸入加強"),   GC::Purple, SC::Linear, 0.5f, 1.f, 4);
        Add(TEXT("purple_puzzle"),      TEXT("謎題挑戰"),       GC::Purple, SC::Linear, 0.f,  1.f, 6);
        Add(TEXT("purple_bullet_ctrl"), TEXT("子彈操控"),       GC::Purple, SC::Linear, 0.f,  1.f, 7);
        Add(TEXT("purple_fps"),         TEXT("第一人稱射擊"),   GC::Purple, SC::Linear, 0.f,  1.f, 8);

        // ── 黃：能力限制（IsRestriction=true）── Godot TotemLibrary.cs:145-150
        Add(TEXT("yellow_cd"),            TEXT("冷卻限制"),   GC::Yellow, SC::Linear, 1.f, 0.f, 5, true);
        Add(TEXT("yellow_mp"),            TEXT("MP消耗限制"), GC::Yellow, SC::Linear, 1.f, 0.f, 5, true);
        Add(TEXT("yellow_range"),         TEXT("射程限制"),   GC::Yellow, SC::Linear, 1.f, 0.f, 4, true);
        Add(TEXT("yellow_hp_cost"),       TEXT("HP代價"),     GC::Yellow, SC::Linear, 2.f, 5.f, 8, true);
        Add(TEXT("yellow_use_condition"), TEXT("使用條件"),   GC::Yellow, SC::Linear, 1.f, 0.f, 6, true);
        Add(TEXT("yellow_negative_cost"), TEXT("負面代價"),   GC::Yellow, SC::Linear, 1.f, 0.f, 7, true);

        // ── 屬性（元素，11 種）── Godot TotemLibrary.cs:153-163
        Add(TEXT("elem_metal"),   TEXT("金屬性"), GC::Elemental, SC::Hyperbolic, 0.8f, 0.f, 4, false, 0, ESET::Metal);
        Add(TEXT("elem_wood"),    TEXT("木屬性"), GC::Elemental, SC::Hyperbolic, 0.8f, 0.f, 4, false, 0, ESET::Wood);
        Add(TEXT("elem_water"),   TEXT("水屬性"), GC::Elemental, SC::Hyperbolic, 0.8f, 0.f, 4, false, 0, ESET::Water);
        Add(TEXT("elem_fire"),    TEXT("火屬性"), GC::Elemental, SC::Hyperbolic, 0.8f, 0.f, 4, false, 0, ESET::Fire);
        Add(TEXT("elem_earth"),   TEXT("土屬性"), GC::Elemental, SC::Hyperbolic, 0.8f, 0.f, 4, false, 0, ESET::Earth);
        Add(TEXT("elem_ice"),     TEXT("冰屬性"), GC::Elemental, SC::Hyperbolic, 1.0f, 0.f, 5, false, 0, ESET::Ice);
        Add(TEXT("elem_wind"),    TEXT("風屬性"), GC::Elemental, SC::Hyperbolic, 1.0f, 0.f, 5, false, 0, ESET::Wind);
        Add(TEXT("elem_light"),   TEXT("光屬性"), GC::Elemental, SC::Hyperbolic, 1.2f, 0.f, 6, false, 0, ESET::Light);
        Add(TEXT("elem_dark"),    TEXT("暗屬性"), GC::Elemental, SC::Hyperbolic, 1.2f, 0.f, 6, false, 0, ESET::Dark);
        Add(TEXT("elem_thunder"), TEXT("雷屬性"), GC::Elemental, SC::Hyperbolic, 1.2f, 0.f, 6, false, 0, ESET::Thunder);
        Add(TEXT("elem_poison"),  TEXT("毒屬性"), GC::Elemental, SC::Hyperbolic, 1.0f, 0.f, 5, false, 0, ESET::Poison);

        // ── 動作（Action，技能因子加入時自動插入）── Godot TotemLibrary.cs:167-195
        const EEngraveCategory Act = EEngraveCategory::Action;
        Add(TEXT("act_area_fan"),         TEXT("扇形衝擊發動"),     GC::Action, SC::Linear, 0.f, 0.f, 0, false, 0, ESET::None, Act, EEngraveTrigger::OnCast);
        Add(TEXT("act_area_around"),      TEXT("周身衝擊發動"),     GC::Action, SC::Linear, 0.f, 0.f, 0, false, 0, ESET::None, Act, EEngraveTrigger::OnCast);
        Add(TEXT("act_area_distant"),     TEXT("遠距圓形衝擊發動"), GC::Action, SC::Linear, 0.f, 0.f, 0, false, 0, ESET::None, Act, EEngraveTrigger::OnCast);
        Add(TEXT("act_area_beam"),        TEXT("射線衝擊發動"),     GC::Action, SC::Linear, 0.f, 0.f, 0, false, 0, ESET::None, Act, EEngraveTrigger::OnCast);
        Add(TEXT("act_technique_sword"),  TEXT("劍技發動"),         GC::Action, SC::Linear, 0.f, 0.f, 0, false, 0, ESET::None, Act, EEngraveTrigger::OnCast);
        Add(TEXT("act_technique_punch"),  TEXT("拳擊發動"),         GC::Action, SC::Linear, 0.f, 0.f, 0, false, 0, ESET::None, Act, EEngraveTrigger::OnCast);
        Add(TEXT("act_technique_shield"), TEXT("盾防發動"),         GC::Action, SC::Linear, 0.f, 0.f, 0, false, 0, ESET::None, Act, EEngraveTrigger::OnCast);
        Add(TEXT("act_projectile_energy"),   TEXT("能量投射發動"), GC::Action, SC::Linear, 0.f, 0.f, 0, false, 0, ESET::None, Act, EEngraveTrigger::OnCast);
        Add(TEXT("act_projectile_physical"), TEXT("實物投射發動"), GC::Action, SC::Linear, 0.f, 0.f, 0, false, 0, ESET::None, Act, EEngraveTrigger::OnCast);
        Add(TEXT("act_passive_continuous"),  TEXT("持續偵測發動"), GC::Action, SC::Linear, 0.f, 0.f, 0, false, 0, ESET::None, Act, EEngraveTrigger::OnTick);
        Add(TEXT("act_passive_switch"),      TEXT("開關切換發動"), GC::Action, SC::Linear, 0.f, 0.f, 0, false, 0, ESET::None, Act, EEngraveTrigger::OnTick);
        Add(TEXT("act_morph_speed"),         TEXT("加速形態發動"), GC::Action, SC::Linear, 0.f, 0.f, 0, false, 0, ESET::None, Act, EEngraveTrigger::OnCast);
        Add(TEXT("act_morph_flight"),        TEXT("飛行形態發動"), GC::Action, SC::Linear, 0.f, 0.f, 0, false, 0, ESET::None, Act, EEngraveTrigger::OnCast);
        Add(TEXT("act_morph_invisible"),     TEXT("隱身形態發動"), GC::Action, SC::Linear, 0.f, 0.f, 0, false, 0, ESET::None, Act, EEngraveTrigger::OnCast);
        Add(TEXT("act_morph_strengthen"),    TEXT("強化形態發動"), GC::Action, SC::Linear, 0.f, 0.f, 0, false, 0, ESET::None, Act, EEngraveTrigger::OnCast);
        Add(TEXT("act_morph_possession"),    TEXT("附體形態發動"), GC::Action, SC::Linear, 0.f, 0.f, 0, false, 0, ESET::None, Act, EEngraveTrigger::OnCast);
        Add(TEXT("act_dash"),                TEXT("衝刺"),         GC::Action, SC::Linear, 0.f, 0.f, 0, false, 0, ESET::None, Act, EEngraveTrigger::OnCast);
        Add(TEXT("act_teleport"),            TEXT("瞬移"),         GC::Action, SC::Linear, 0.f, 0.f, 0, false, 0, ESET::None, Act, EEngraveTrigger::OnCast);
        Add(TEXT("act_dodge"),               TEXT("閃避翻滾"),     GC::Action, SC::Linear, 0.f, 0.f, 0, false, 0, ESET::None, Act, EEngraveTrigger::OnCast);
        Add(TEXT("act_portal"),              TEXT("傳送門建立"),   GC::Action, SC::Linear, 0.f, 0.f, 0, false, 0, ESET::None, Act, EEngraveTrigger::OnCast);
        Add(TEXT("act_summon_minion"),       TEXT("召喚精靈發動"), GC::Action, SC::Linear, 0.f, 0.f, 0, false, 0, ESET::None, Act, EEngraveTrigger::OnCast);
        Add(TEXT("act_summon_turret"),       TEXT("召喚砲台發動"), GC::Action, SC::Linear, 0.f, 0.f, 0, false, 0, ESET::None, Act, EEngraveTrigger::OnCast);
        Add(TEXT("act_summon_guardian"),     TEXT("召喚護衛發動"), GC::Action, SC::Linear, 0.f, 0.f, 0, false, 0, ESET::None, Act, EEngraveTrigger::OnCast);
        Add(TEXT("act_summon_weapon"),       TEXT("召喚武器發動"), GC::Action, SC::Linear, 0.f, 0.f, 0, false, 0, ESET::None, Act, EEngraveTrigger::OnCast);
        Add(TEXT("act_summon_building"),     TEXT("召喚建築發動"), GC::Action, SC::Linear, 0.f, 0.f, 0, false, 0, ESET::None, Act, EEngraveTrigger::OnCast);
        Add(TEXT("act_summon_vehicle"),      TEXT("召喚載具發動"), GC::Action, SC::Linear, 0.f, 0.f, 0, false, 0, ESET::None, Act, EEngraveTrigger::OnCast);
        Add(TEXT("act_domain_barrier"),      TEXT("結界領域發動"), GC::Action, SC::Linear, 0.f, 0.f, 0, false, 0, ESET::None, Act, EEngraveTrigger::OnCast);
        Add(TEXT("act_domain_terrain"),      TEXT("地形改造發動"), GC::Action, SC::Linear, 0.f, 0.f, 0, false, 0, ESET::None, Act, EEngraveTrigger::OnCast);
        Add(TEXT("act_domain_weather"),      TEXT("天候操控發動"), GC::Action, SC::Linear, 0.f, 0.f, 0, false, 0, ESET::None, Act, EEngraveTrigger::OnCast);

        // ── 法則（14 種，LV 50/65/80/100 解鎖）── Godot TotemLibrary.cs:199-212
        Add(TEXT("law_time"),      TEXT("時間"), GC::Law, SC::Hyperbolic, 0.5f, 0.f, 15, false, 50);
        Add(TEXT("law_space"),     TEXT("空間"), GC::Law, SC::Hyperbolic, 0.5f, 0.f, 15, false, 50);
        Add(TEXT("law_genesis"),   TEXT("造化"), GC::Law, SC::Hyperbolic, 0.5f, 0.f, 15, false, 50);
        Add(TEXT("law_causality"), TEXT("因果"), GC::Law, SC::Hyperbolic, 0.5f, 0.f, 15, false, 50);
        Add(TEXT("law_cycle"),      TEXT("輪迴"), GC::Law, SC::Hyperbolic, 0.3f, 0.f, 20, false, 65);
        Add(TEXT("law_life_death"), TEXT("生死"), GC::Law, SC::Hyperbolic, 0.3f, 0.f, 20, false, 65);
        Add(TEXT("law_soul"),       TEXT("靈魂"), GC::Law, SC::Hyperbolic, 0.3f, 0.f, 20, false, 65);
        Add(TEXT("law_yin_yang"),  TEXT("陰陽"), GC::Law, SC::Hyperbolic, 0.3f, 0.f, 20, false, 65);
        Add(TEXT("law_extreme"),   TEXT("極點"), GC::Law, SC::Hyperbolic, 0.2f, 0.f, 25, false, 80);
        Add(TEXT("law_infinite"),  TEXT("無量"), GC::Law, SC::Hyperbolic, 0.2f, 0.f, 25, false, 80);
        Add(TEXT("law_create"),    TEXT("創造"), GC::Law, SC::Hyperbolic, 0.2f, 0.f, 25, false, 80);
        Add(TEXT("law_dimension"), TEXT("次元"), GC::Law, SC::Hyperbolic, 0.2f, 0.f, 25, false, 80);
        Add(TEXT("law_world"),     TEXT("世界"), GC::Law, SC::Hyperbolic, 0.1f, 0.f, 30, false, 100);
        Add(TEXT("law_chaos"),     TEXT("混沌"), GC::Law, SC::Hyperbolic, 0.1f, 0.f, 30, false, 100);

        return E;
    }();
    return Data;
}

const TSet<FName>& FTotemLibrary::ContainerActionIds()
{
    // Godot TotemLibrary.cs:219-229
    static const TSet<FName> Ids = {
        TEXT("act_projectile_energy"), TEXT("act_projectile_physical"),
        TEXT("act_summon_minion"),     TEXT("act_summon_turret"),
        TEXT("act_summon_guardian"),   TEXT("act_summon_weapon"),
        TEXT("act_summon_building"),   TEXT("act_summon_vehicle"),
    };
    return Ids;
}

const TMap<FName, FName>& FTotemLibrary::DefaultActionEngraveId()
{
    // Godot TotemLibrary.cs:235-266
    static const TMap<FName, FName> Map = {
        { TEXT("area_fan"),            TEXT("act_area_fan") },
        { TEXT("area_around"),         TEXT("act_area_around") },
        { TEXT("area_distant"),        TEXT("act_area_distant") },
        { TEXT("area_beam"),           TEXT("act_area_beam") },
        { TEXT("technique_sword"),     TEXT("act_technique_sword") },
        { TEXT("technique_punch"),     TEXT("act_technique_punch") },
        { TEXT("technique_shield"),    TEXT("act_technique_shield") },
        { TEXT("projectile_energy"),   TEXT("act_projectile_energy") },
        { TEXT("projectile_physical"), TEXT("act_projectile_physical") },
        { TEXT("passive_continuous"),  TEXT("act_passive_continuous") },
        { TEXT("passive_switch"),      TEXT("act_passive_switch") },
        { TEXT("morph_speed"),         TEXT("act_morph_speed") },
        { TEXT("morph_flight"),        TEXT("act_morph_flight") },
        { TEXT("morph_invisible"),     TEXT("act_morph_invisible") },
        { TEXT("morph_strengthen"),    TEXT("act_morph_strengthen") },
        { TEXT("morph_possession"),    TEXT("act_morph_possession") },
        { TEXT("displace_dash"),       TEXT("act_dash") },
        { TEXT("displace_teleport"),   TEXT("act_teleport") },
        { TEXT("displace_dodge"),      TEXT("act_dodge") },
        { TEXT("displace_portal"),     TEXT("act_portal") },
        { TEXT("summon_minion"),       TEXT("act_summon_minion") },
        { TEXT("summon_turret"),       TEXT("act_summon_turret") },
        { TEXT("summon_guardian"),     TEXT("act_summon_guardian") },
        { TEXT("summon_weapon"),       TEXT("act_summon_weapon") },
        { TEXT("summon_building"),     TEXT("act_summon_building") },
        { TEXT("summon_vehicle"),      TEXT("act_summon_vehicle") },
        { TEXT("domain_barrier"),      TEXT("act_domain_barrier") },
        { TEXT("domain_terrain"),      TEXT("act_domain_terrain") },
        { TEXT("domain_weather"),      TEXT("act_domain_weather") },
    };
    return Map;
}

const FTotemData* FTotemLibrary::FindTotem(FName Id)
{
    for (const FTotemData& T : AllTotems())
        if (T.TotemId == Id) return &T;
    return nullptr;
}

const FEngraveData* FTotemLibrary::FindEngraving(FName Id)
{
    for (const FEngraveData& E : AllEngravings())
        if (E.EngraveId == Id) return &E;
    return nullptr;
}
