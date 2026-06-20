/* Purpose: Independent material pouch, cooking and alchemy */

#include "angband.h"

enum {
    MAT_IRON_ORE,
    MAT_SILVER_ORE,
    MAT_MITHRIL_DUST,
    MAT_CRYSTAL_SHARD,
    MAT_HERB,
    MAT_BEAST_MEAT,
    MAT_DRAGON_SCALE,
    MAT_DEMON_ICHOR,
    MAT_ARCANE_ESSENCE,
    MAT_RARE_CATALYST
};

typedef struct {
    cptr name;
    cptr source;
    cptr use;
    int value;
} material_info_t;

typedef struct {
    int id;
    int amount;
} material_cost_t;

typedef struct {
    char key;
    cptr name;
    cptr desc;
    material_cost_t costs[5];
    void (*effect)(void);
} material_recipe_t;

static material_info_t _materials[MATERIAL_MAX] = {
    { "铁矿石",     "挖掘矿脉",       "基础炼金和工具性配方", 1 },
    { "银矿砂",     "深层矿脉",       "中级炼金材料",         2 },
    { "秘银粉尘",   "深层矿脉",       "高级炼金材料",         5 },
    { "水晶碎片",   "矿脉和构装体",   "稳定药剂和魔力料理",   4 },
    { "草药",       "野兽和自然生物", "烹饪和治疗药剂",       1 },
    { "兽肉",       "野兽和活物",     "烹饪",                 1 },
    { "龙鳞",       "龙类",           "抗性料理和高级药剂",   6 },
    { "恶魔脓液",   "恶魔",           "危险炼金材料",         5 },
    { "奥术灵质",   "亡灵和魔法生物", "速度、抗性和高级药剂", 7 },
    { "稀有触媒",   "强大敌人",       "顶级炼金材料",        12 },
};

static bool _valid_material(int id)
{
    return 0 <= id && id < MATERIAL_MAX;
}

cptr material_name(int id)
{
    if (!_valid_material(id)) return "";
    return _materials[id].name;
}

static bool _has_tool(int sval)
{
    return pack_find_obj(TV_JUNK, sval) || equip_find_obj(TV_JUNK, sval);
}

static void _add_material(int id, int amount, bool quiet)
{
    if (!_valid_material(id) || amount <= 0) return;

    if (p_ptr->materials[id] > 2000000000L - amount)
        p_ptr->materials[id] = 2000000000L;
    else
        p_ptr->materials[id] += amount;

    if (!quiet)
        msg_format("你获得了 %d 份%s。", amount, material_name(id));
}

void materials_add(int id, int amount)
{
    _add_material(id, amount, FALSE);
}

int materials_count(int id)
{
    if (!_valid_material(id)) return 0;
    return p_ptr->materials[id];
}

static bool _can_pay(material_cost_t *costs)
{
    int i;
    for (i = 0; costs[i].id >= 0; i++)
    {
        if (!_valid_material(costs[i].id)) return FALSE;
        if (p_ptr->materials[costs[i].id] < costs[i].amount) return FALSE;
    }
    return TRUE;
}

static void _pay(material_cost_t *costs)
{
    int i;
    for (i = 0; costs[i].id >= 0; i++)
        p_ptr->materials[costs[i].id] -= costs[i].amount;
}

static void _print_costs(material_cost_t *costs, int row, int col)
{
    int i;
    bool first = TRUE;
    char buf[160];

    buf[0] = '\0';
    for (i = 0; costs[i].id >= 0; i++)
    {
        char tmp[40];
        sprintf(tmp, "%s%s x%d", first ? "" : ", ", material_name(costs[i].id), costs[i].amount);
        strcat(buf, tmp);
        first = FALSE;
    }
    prt(buf, row, col);
}

static int _long_dur(void)
{
    return 800 + p_ptr->lev * 30;
}

static int _longer_dur(void)
{
    return 1200 + p_ptr->lev * 40;
}

static void _cook_hearty_stew(void)
{
    int dur = _longer_dur();
    set_hero(MAX(p_ptr->hero, dur), FALSE);
    set_tim_regen(MAX(p_ptr->tim_regen, dur), FALSE);
    msg_print("这份热汤让你精神振奋。");
}

static void _cook_miner_meal(void)
{
    int dur = _long_dur();
    set_blessed(MAX(p_ptr->blessed, dur), FALSE);
    set_tim_infra(MAX(p_ptr->tim_infra, dur), FALSE);
    msg_print("矿工餐让你的感官更加敏锐。");
}

static void _cook_dragon_roast(void)
{
    int dur = _long_dur();
    set_oppose_base(MAX(p_ptr->oppose_acid, dur), FALSE);
    set_shield(MAX(p_ptr->shield, dur), FALSE);
    msg_print("龙鳞烤肉在你体内留下了炽热的守护感。");
}

static void _cook_arcane_broth(void)
{
    int dur = 600 + p_ptr->lev * 20;
    set_fast(MAX(p_ptr->fast, dur), FALSE);
    set_tim_regen(MAX(p_ptr->tim_regen, dur), FALSE);
    msg_print("灵质羹让你的动作变得轻快。");
}

static void _make_potion(int sval, int number)
{
    object_type forge = {0};
    int k_idx = lookup_kind(TV_POTION, sval);

    if (!k_idx)
    {
        msg_print("这个药水配方暂时无法完成。");
        return;
    }

    object_prep(&forge, k_idx);
    forge.number = number;
    object_aware(&forge);
    identify_item(&forge);
    pack_carry(&forge);
    msg_print("你调制出了一瓶药水。");
}

static void _brew_cure_critical(void) { _make_potion(SV_POTION_CURE_CRITICAL, 1); }
static void _brew_curing(void) { _make_potion(SV_POTION_CURING, 1); }
static void _brew_speed(void) { _make_potion(SV_POTION_SPEED, 1); }
static void _brew_resistance(void) { _make_potion(SV_POTION_RESISTANCE, 1); }
static void _brew_healing(void) { _make_potion(SV_POTION_HEALING, 1); }
static void _brew_rare_catalyst(void)
{
    _add_material(MAT_RARE_CATALYST, 1, TRUE);
    msg_print("你炼成了 1 份稀有触媒。");
}

static material_recipe_t _cooking[] = {
    { 'a', "草药炖肉", "长期英雄气概和再生。",
        { { MAT_BEAST_MEAT, 3 }, { MAT_HERB, 2 }, { -1, 0 } }, _cook_hearty_stew },
    { 'b', "矿工热餐", "长期祝福和红外视觉。",
        { { MAT_BEAST_MEAT, 2 }, { MAT_IRON_ORE, 2 }, { MAT_HERB, 1 }, { -1, 0 } }, _cook_miner_meal },
    { 'c', "龙鳞烤肉", "长期基础抗性和护盾。",
        { { MAT_BEAST_MEAT, 2 }, { MAT_DRAGON_SCALE, 1 }, { MAT_HERB, 3 }, { MAT_RARE_CATALYST, 1 }, { -1, 0 } }, _cook_dragon_roast },
    { 'd', "灵质羹", "长期加速和再生。",
        { { MAT_ARCANE_ESSENCE, 1 }, { MAT_CRYSTAL_SHARD, 2 }, { MAT_HERB, 2 }, { MAT_RARE_CATALYST, 1 }, { -1, 0 } }, _cook_arcane_broth },
    { 0, NULL, NULL, { { -1, 0 } }, NULL }
};

static material_recipe_t _alchemy[] = {
    { 'a', "治疗重伤药水", "制造一瓶治疗重伤药水。",
        { { MAT_HERB, 2 }, { MAT_BEAST_MEAT, 1 }, { -1, 0 } }, _brew_cure_critical },
    { 'b', "治愈药水", "制造一瓶治愈药水。",
        { { MAT_HERB, 3 }, { MAT_CRYSTAL_SHARD, 1 }, { -1, 0 } }, _brew_curing },
    { 'c', "速度药水", "制造一瓶速度药水。",
        { { MAT_ARCANE_ESSENCE, 1 }, { MAT_CRYSTAL_SHARD, 2 }, { MAT_HERB, 2 }, { -1, 0 } }, _brew_speed },
    { 'd', "抗性药水", "制造一瓶抗性药水。",
        { { MAT_DRAGON_SCALE, 1 }, { MAT_DEMON_ICHOR, 1 }, { MAT_CRYSTAL_SHARD, 2 }, { -1, 0 } }, _brew_resistance },
    { 'e', "治疗药水", "制造一瓶治疗药水。",
        { { MAT_HERB, 4 }, { MAT_ARCANE_ESSENCE, 1 }, { -1, 0 } }, _brew_healing },
    { 'f', "稀有触媒", "将高阶材料炼成 1 份稀有触媒。",
        { { MAT_MITHRIL_DUST, 2 }, { MAT_CRYSTAL_SHARD, 2 }, { MAT_DRAGON_SCALE, 1 }, { MAT_ARCANE_ESSENCE, 1 }, { -1, 0 } }, _brew_rare_catalyst },
    { 0, NULL, NULL, { { -1, 0 } }, NULL }
};

static void _run_recipes(cptr title, material_recipe_t *recipes, int tool_sval)
{
    int i, row;
    int cmd;

    if (!_has_tool(tool_sval))
    {
        msg_format("你需要%s。", tool_sval == SV_JUNK_COOKING_KIT ? "烹饪工具" : "炼金器具");
        return;
    }

    while (1)
    {
        Term_clear();
        c_prt(TERM_L_BLUE, title, 1, 2);
        row = 3;
        for (i = 0; recipes[i].key; i++)
        {
            prt(format("(%c) %s - %s", recipes[i].key, recipes[i].name, recipes[i].desc), row++, 2);
            _print_costs(recipes[i].costs, row++, 6);
        }
        prt("ESC) 返回", 22, 2);
        prt("命令: ", 21, 0);

        cmd = inkey();
        if (cmd == ESCAPE) break;

        for (i = 0; recipes[i].key; i++)
        {
            if (cmd == recipes[i].key)
            {
                if (!_can_pay(recipes[i].costs))
                {
                    msg_print("材料不足。");
                    msg_print(NULL);
                    break;
                }
                _pay(recipes[i].costs);
                recipes[i].effect();
                msg_print(NULL);
                break;
            }
        }
        if (!recipes[i].key) bell();
    }
}

static int _material_from_key(int key)
{
    if ('a' <= key && key < 'a' + MATERIAL_MAX)
        return key - 'a';
    return -1;
}

static void _convert_material(void)
{
    int src, dst, out;
    int cmd;

    if (!_has_tool(SV_JUNK_ALCHEMY_KIT))
    {
        msg_print("你需要炼金器具。");
        return;
    }

    Term_clear();
    c_prt(TERM_L_BLUE, "材料转化", 1, 2);
    prt("选择要分解的高价值材料。每次消耗 1 份，按约 75% 价值转成低价值材料。", 3, 2);
    for (src = 0; src < MATERIAL_MAX; src++)
        prt(format("(%c) %-12s 数量:%6d 价值:%2d", 'a' + src, material_name(src), p_ptr->materials[src], _materials[src].value), 5 + src, 2);
    prt("ESC) 返回", 22, 2);
    prt("来源: ", 21, 0);
    cmd = inkey();
    if (cmd == ESCAPE) return;
    src = _material_from_key(cmd);
    if (!_valid_material(src) || p_ptr->materials[src] <= 0 || _materials[src].value <= 1)
    {
        msg_print("这个材料不能分解。");
        return;
    }

    Term_clear();
    c_prt(TERM_L_BLUE, "材料转化", 1, 2);
    prt(format("来源: %s。选择价值更低的目标材料。", material_name(src)), 3, 2);
    for (dst = 0; dst < MATERIAL_MAX; dst++)
    {
        if (_materials[dst].value < _materials[src].value)
            prt(format("(%c) %-12s 数量:%6d 价值:%2d", 'a' + dst, material_name(dst), p_ptr->materials[dst], _materials[dst].value), 5 + dst, 2);
    }
    prt("ESC) 返回", 22, 2);
    prt("目标: ", 21, 0);
    cmd = inkey();
    if (cmd == ESCAPE) return;
    dst = _material_from_key(cmd);
    if (!_valid_material(dst) || _materials[dst].value >= _materials[src].value)
    {
        msg_print("只能转化为价值更低的材料。");
        return;
    }

    out = MAX(1, (_materials[src].value * 3 + 4 * _materials[dst].value - 1) / (4 * _materials[dst].value));
    p_ptr->materials[src]--;
    _add_material(dst, out, TRUE);
    msg_format("你将 1 份%s转化为了 %d 份%s。", material_name(src), out, material_name(dst));
}

void do_cmd_knowledge_materials(void)
{
    int i, cmd;

    while (1)
    {
        Term_clear();
        c_prt(TERM_L_BLUE, "材料与制作", 1, 2);
        prt("材料不会占用物品栏空间。烹饪和炼金需要在杂货店购买对应工具。", 2, 2);
        c_prt(TERM_YELLOW, "键", 4, 2);
        c_prt(TERM_YELLOW, "材料", 4, 7);
        c_prt(TERM_YELLOW, "数量", 4, 22);
        c_prt(TERM_YELLOW, "价值", 4, 32);
        c_prt(TERM_YELLOW, "来源与用途", 4, 40);
        for (i = 0; i < MATERIAL_MAX; i++)
        {
            int row = 5 + i;
            c_prt(TERM_WHITE, format("%c)", 'a' + i), row, 2);
            c_prt(TERM_WHITE, material_name(i), row, 7);
            c_prt(TERM_WHITE, format("%6d", p_ptr->materials[i]), row, 20);
            c_prt(TERM_WHITE, format("%4d", _materials[i].value), row, 31);
            c_prt(TERM_WHITE, format("%s；%s", _materials[i].source, _materials[i].use), row, 40);
        }
        prt(format("工具: 烹饪工具[%s]  炼金器具[%s]",
            _has_tool(SV_JUNK_COOKING_KIT) ? "有" : "无",
            _has_tool(SV_JUNK_ALCHEMY_KIT) ? "有" : "无"), 17, 2);
        prt("(c) 烹饪  (a) 炼药  (t) 材料转化  ESC) 返回", 19, 2);
        prt("命令: ", 21, 0);

        cmd = inkey();
        if (cmd == ESCAPE) break;
        if (cmd == 'c') _run_recipes("烹饪", _cooking, SV_JUNK_COOKING_KIT);
        else if (cmd == 'a') _run_recipes("炼药", _alchemy, SV_JUNK_ALCHEMY_KIT);
        else if (cmd == 't')
        {
            _convert_material();
            msg_print(NULL);
        }
        else bell();
    }
}

void materials_on_mined(feature_type *old_f_ptr)
{
    int mining, amount;

    if (!character_dungeon || !old_f_ptr) return;

    mining = skills_mining_current();
    amount = 1 + MAX(0, dun_level) / 25 + mining / 1600;

    if (have_flag(old_f_ptr->flags, FF_HAS_GOLD))
    {
        materials_add(MAT_IRON_ORE, amount);
        if (dun_level >= 20 && one_in_(3)) materials_add(MAT_SILVER_ORE, 1 + dun_level / 50);
        if (dun_level >= 40 && randint0(100) < 12 + mining / 500) materials_add(MAT_MITHRIL_DUST, 1);
    }
    else if (have_flag(old_f_ptr->flags, FF_MAY_HAVE_GOLD))
    {
        if (!one_in_(3)) materials_add(MAT_IRON_ORE, MAX(1, amount / 2));
        if (dun_level >= 30 && one_in_(6)) materials_add(MAT_CRYSTAL_SHARD, 1);
    }
}

void materials_on_monster_death(monster_type *m_ptr, bool eligible)
{
    monster_race *r_ptr;
    int level;

    if (!eligible || !m_ptr) return;

    r_ptr = &r_info[m_ptr->r_idx];
    level = MAX(1, r_ptr->level);

    if ((r_ptr->flags3 & RF3_ANIMAL) && randint0(100) < 35 + level / 2)
    {
        materials_add(MAT_BEAST_MEAT, 1 + level / 35);
        if (one_in_(3)) materials_add(MAT_HERB, 1);
    }
    else if (!(r_ptr->flags3 & (RF3_UNDEAD | RF3_DEMON | RF3_NONLIVING)) && randint0(100) < 10 + level / 3)
        materials_add(MAT_BEAST_MEAT, 1);

    if ((r_ptr->flags3 & RF3_DRAGON) && randint0(100) < 45 + level / 2)
        materials_add(MAT_DRAGON_SCALE, 1 + level / 50);

    if ((r_ptr->flags3 & RF3_DEMON) && randint0(100) < 35 + level / 2)
        materials_add(MAT_DEMON_ICHOR, 1 + level / 60);

    if ((r_ptr->flags3 & (RF3_UNDEAD | RF3_NONLIVING)) && randint0(100) < 25 + level / 2)
        materials_add(MAT_ARCANE_ESSENCE, 1);

    if ((r_ptr->flags2 & (RF2_AURA_FIRE | RF2_AURA_ELEC)) && one_in_(4))
        materials_add(MAT_CRYSTAL_SHARD, 1);

    if (level >= 40 && randint0(100) < MIN(25, level / 5))
        materials_add(MAT_RARE_CATALYST, 1);
}
