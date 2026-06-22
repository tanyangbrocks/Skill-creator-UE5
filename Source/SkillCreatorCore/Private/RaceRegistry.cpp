#include "RaceRegistry.h"

// 資料來源：Import word setting/ExportBlock-game-word/ 下 16 份「體系」索引文件，逐一讀過後
// 整理進計畫文件 docs/plan-w10-character-creation.md §3.1.1，這裡照那份清單轉成程式碼。
//
// 2026-06-22 第一輪修正：原本「只收錄文件裡有現成簡述的種族」這個篩選規則被使用者指出有
// 遺漏（鬼族、哥布林族等），逐一開對應子文件重新核對後發現：部份「連結待補」的種族其實有
// 獨立子文件、內容完整（例如鬼族有紫/赤/青三篇、哥布林族、獸人族、歐克族都有專門文件），
// 只是當初索引文件本身只放連結沒放內文，沒被前一輪讀到。改為：只要文件裡「寫出名稱」就一律
// 註冊，有敘述的照實摘要，真的只有名稱、查無內文的標註「（敘述待補）」（不是空白，是明確
// 告知 UI 顯示「這個種族還沒寫敘述」，跟「文件裡有敘述只是還沒摘要」要區分清楚）。
//
// 2026-06-22 第二輪修正：使用者又指出兩個問題——① 第一輪新增的種族全部「附加在每個體系
// 清單最後面」，沒有照原文件順序插入正確位置；② 人、類人體系其實還有玄階/地階的種族
// （瓦爾基利、龍人、不滅族、古蘭族、阿修羅），第一輪完全沒讀到（之前依賴的計畫文件摘要
// 寫「人、類人體系（黃階，全部有現成敘述）」，這句摘要本身就是錯的——直接重新打開全部
// 16 份體系總覽文件逐一核對才發現）。這次把每個體系的 R() 呼叫順序都改成跟文件一致（同一
// 階層內按文件出現順序），並補上人、類人體系玄階/地階 5 族（文件裡是內文敘述，不是連結，
// 不需要再開子文件）。
//
// 跨體系重複出現的種族（森精族＝人類人/精怪、人魚族＝人類人/水行、邪根族＝半神/邪靈）
// 各自只在一個 SystemId 下註冊一次，註冊位置選擇内容/物理描述最貼合、且該體系文件本身就
// 列出完整敘述的那一側，並在該行註解標明另一邊的交叉引用。

static void Add(TMap<FName, FRaceDefinition>& Out, FName Id, FName SystemId,
                 const FText& Name, ERaceTier Tier, const FText& Desc)
{
    FRaceDefinition D;
    D.Id = Id; D.SystemId = SystemId; D.DisplayName = Name; D.Tier = Tier; D.Description = Desc;
    Out.Add(Id, MoveTemp(D));
}

void FRaceRegistry::Init(TArray<FRaceSystemDefinition>& OutSystems, TMap<FName, FRaceDefinition>& OutRaces)
{
    auto Sys = [&](FName Id, const FText& Name)
    {
        OutSystems.Add(FRaceSystemDefinition{ Id, Name });
    };
    auto R = [&](FName Id, FName SystemId, const FText& Name, ERaceTier Tier, const FText& Desc)
    {
        Add(OutRaces, Id, SystemId, Name, Tier, Desc);
    };
    using ERT = ERaceTier;

    // ── 1. 人、類人體系（黃階→玄階→地階，照文件順序）────────────────────────
    Sys(TEXT("Human"), INVTEXT("人、類人體系"));
    // —黃階—
    R(TEXT("Human"),    TEXT("Human"), INVTEXT("人族"),   ERT::Yellow, INVTEXT("最強智慧與魔力掌控，肉體偏弱，十二大能量都有一定天賦，召喚術/煉金術/科技概念創造者。"));
    R(TEXT("Dwarf"),    TEXT("Human"), INVTEXT("矮人族"), ERT::Yellow, INVTEXT("與人族共存，身高較矮，鬍鬚濃密且耐火，酒量好，肉體能力、五感較強，擅工匠、商務。"));
    R(TEXT("Gnome"),    TEXT("Human"), INVTEXT("侏儒族"), ERT::Yellow, INVTEXT("矮人與人類血脈各占一半，又稱半矮人，非常喜愛金錢，善貿易、商務、情報蒐集，智力偏高。"));
    R(TEXT("Halfling"), TEXT("Human"), INVTEXT("哈頓族"), ERT::Yellow, INVTEXT("鼻尖長、膚色偏紅，身高偏矮，智力與體能稍弱，具靈玉者靈力天賦高，祖先有惡魔血脈。"));
    R(TEXT("LongEar"),  TEXT("Human"), INVTEXT("長耳族"), ERT::Yellow, INVTEXT("長耳，膚色較白、顏值極高，壽命較長，魔力天賦強，與精靈契合度高，祖先有精靈血脈。"));
    R(TEXT("RedHunter"),TEXT("Human"), INVTEXT("紅獵族"), ERT::Yellow, INVTEXT("白髮、尖耳、膚色赤紅，肉體強大，感官發達，喜愛戰鬥，善於狩獵的特殊種族。"));
    R(TEXT("HalfDemonHuman"),TEXT("Human"), INVTEXT("半妖族"), ERT::Yellow, INVTEXT("（敘述待補）"));
    // 森精族（文件原文位置在半妖族之後、狼人族之前）兼具人類人/精怪體系特質，內容/敘述在
    // 精怪體系文件更完整，註冊在 Spirit 體系下，見該區塊。
    R(TEXT("Werewolf"),  TEXT("Human"), INVTEXT("狼人族"), ERT::Yellow, INVTEXT("平時與常人無異，但在月圓之夜會化身為力量強大的狼人，獵殺人類；成功捕獵的人越多，自身抵抗狼人化的力量就越強，最終可能完全變回人類。"));
    R(TEXT("Dhampir"),   TEXT("Human"), INVTEXT("半血族"), ERT::Yellow, INVTEXT("人族與血族的後代，雖不懼陽光但需吸食血肉維生，普遍顏值極高、膚色蒼白，智力、肉體素質和恢復力特別強大，壽命也更長。"));
    R(TEXT("HumanGhoulKin"),TEXT("Human"), INVTEXT("人鬼族"), ERT::Yellow, INVTEXT("祖先為鬼族與人類的後代，外貌與人類無異但以食人/類人為生，肉體、五感、智慧、外貌及操縱能量天賦都高於人族，吞食人類可快速獲得力量，通常暗中潛伏於人族領地，被發現格殺勿論。"));
    // 人魚族／翼人族（文件列為「跨體系類人物種」）內容/敘述分別在水行/天行體系文件更完整，
    // 註冊在 Water/Sky 體系下，見對應區塊。
    // —玄階—（2026-06-22 補：原計畫文件摘要誤寫「人、類人體系全黃階」，重讀原文件才發現）
    R(TEXT("Valkyrie"),    TEXT("Human"), INVTEXT("瓦爾基利"), ERT::Profound, INVTEXT("天使後裔，戰鬥天賦、正義感強，與天使契合度高，地位尊貴，情緒激動時體表會亮起古老玄奧的光紋。"));
    R(TEXT("DragonHuman"), TEXT("Human"), INVTEXT("龍人"),     ERT::Profound, INVTEXT("人與龍的後代，可使用部份龍的力量，且天生必定同時擁有魔力和靈力，並將其結合為䵻龗。"));
    // —地階—
    R(TEXT("Undying"), TEXT("Human"), INVTEXT("不滅族"), ERT::Earth, INVTEXT("世界孕育的不死者，容貌俊美而空靈，唯一的弱點是精神攻擊，每個體擁有一個附加天賦，極難生育。"));
    R(TEXT("Gulan"),    TEXT("Human"), INVTEXT("古蘭族"), ERT::Earth, INVTEXT("擁有堪比天球族的恐怖魔力，智力極高、肉體力量也不俗，平均實力甚至能比肩地階半神體系，但每名個體天生背負一種可怕詛咒（如沒有情感、遭萬物生靈憎恨、終生沉浸於悲傷等），大多無法融入人類社會。"));
    R(TEXT("Asura"),    TEXT("Human"), INVTEXT("阿修羅"), ERT::Earth, INVTEXT("有六隻手臂，肉體力大無窮，善用真氣、靈力，生育難度高，但該族性情暴躁好戰，男性貌醜且嗔怒心重、女性貌美但嫉妒心重，經常與異族發起戰爭。"));

    // ── 2. 精怪體系（黃階→玄階→地階，照文件順序）────────────────────────────
    Sys(TEXT("Spirit"), INVTEXT("精怪體系"));
    R(TEXT("Dryad"),         TEXT("Spirit"), INVTEXT("樹妖族"), ERT::Yellow, INVTEXT("木本植物成精，肉體和靈力天賦強，多在叢林生活，具備靈智的樹木皆可歸為樹妖族，作為半天之巔的主宰，異族不會輕易靠近。"));
    R(TEXT("FlowerSpirit"),  TEXT("Spirit"), INVTEXT("花妖族"), ERT::Yellow, INVTEXT("草本植物成精，顏值和魔法天賦強，多在叢林生活。"));
    R(TEXT("MushroomSpirit"),TEXT("Spirit"), INVTEXT("蕈菇族"), ERT::Yellow, INVTEXT("真菌類生物成精，具強大妖力和魔法天賦，擁有「寄生」種族固有技，部份含劇毒或為珍稀草藥。"));
    R(TEXT("Pixie"),         TEXT("Spirit"), INVTEXT("妖精族"), ERT::Yellow, INVTEXT("（敘述待補）"));
    R(TEXT("EarthSpirit"),   TEXT("Spirit"), INVTEXT("地精族"), ERT::Yellow, INVTEXT("居住在地底的特殊精怪，普遍身形矮小但體型壯碩，具備強大魔力。"));
    R(TEXT("ForestSpiritKin"),TEXT("Spirit"),INVTEXT("森精族"), ERT::Yellow, INVTEXT("兼具類人與精怪體系特質，與精靈契合度極高，膚色普遍白皙、顏值極高，多居住於森林中的祕境，喜愛種植花草樹木，善用弓箭（人、類人體系文件亦有交叉引用）。"));
    R(TEXT("DemonSpirit"),   TEXT("Spirit"), INVTEXT("魔靈族"), ERT::Yellow, INVTEXT("擁有魔力之事物迷宮化成功後誕生靈智，具備強大魔力。"));
    R(TEXT("FairySpirit"),   TEXT("Spirit"), INVTEXT("仙靈族"), ERT::Yellow, INVTEXT("擁有仙道靈力之事物迷宮化後誕生靈智，具備強大靈力。"));
    R(TEXT("VesselSpirit"),  TEXT("Spirit"), INVTEXT("器靈族"), ERT::Yellow, INVTEXT("各種器物迷宮化後產生靈智而幻形，分佈於世界各地，有眾多分支。"));
    R(TEXT("BuildingSpirit"),TEXT("Spirit"), INVTEXT("建樓族"), ERT::Yellow, INVTEXT("建築物迷宮化成功後誕生靈智，族群數量稀少，分佈位置不固定，有些不能移動。"));
    R(TEXT("WeaponSpirit"),  TEXT("Spirit"), INVTEXT("神兵族"), ERT::Yellow, INVTEXT("武器被鍛造後經殺戮、蘊養或悠久歷史而誕生自我意識並幻形，有劍族、刀族等分支。"));
    R(TEXT("Succubus"),      TEXT("Spirit"), INVTEXT("魅魔族"), ERT::Yellow, INVTEXT("顏值極高，藉由魅惑其它種族來交配，進而吸取其生命能量茁壯自身。"));
    R(TEXT("LightKin"),      TEXT("Spirit"), INVTEXT("光族"),   ERT::Yellow, INVTEXT("通體發光的種族，與靈界物種契合度高，能變化為各種型態。"));
    R(TEXT("SevenColor"),    TEXT("Spirit"), INVTEXT("七色族"), ERT::Yellow, INVTEXT("有七色支派，彼此相處時力量會變渾濁，七色齊聚形成黑色時將產生極大力量。"));
    R(TEXT("FlameSpirit"),   TEXT("Spirit"), INVTEXT("炎靈族"), ERT::Profound, INVTEXT("火元素半靈體，多生活在火山、熔岩地區，魔核被擊毀後會直接死亡。"));
    R(TEXT("FrostSpirit"),   TEXT("Spirit"), INVTEXT("冰魄族"), ERT::Profound, INVTEXT("冰元素半靈體，多生活在極地、雪山等極寒地區，魔核被擊毀後會直接死亡。"));
    R(TEXT("ThunderSpirit"), TEXT("Spirit"), INVTEXT("雷鳴族"), ERT::Profound, INVTEXT("雷元素半靈體，多生活在雷暴、沼澤、山地地區，魔核被擊毀後會直接死亡。"));
    R(TEXT("EarthenSpirit"), TEXT("Spirit"), INVTEXT("厚土族"), ERT::Profound, INVTEXT("土元素半靈體，多生活在沙漠、荒野地區，魔核被擊毀後會直接死亡。"));
    R(TEXT("Druid"),         TEXT("Spirit"), INVTEXT("德魯伊族"),ERT::Profound, INVTEXT("（敘述待補）"));
    R(TEXT("Nymph"),         TEXT("Spirit"), INVTEXT("寧芙族"), ERT::Earth, INVTEXT("屬於半神生命，喜歡歌舞、壽命無盡，容貌十分美麗，自然孕育出的生命，分佈於世界各處。"));

    // ── 3. 寄生體系 ──────────────────────────────────────────────────────────
    Sys(TEXT("Parasite"), INVTEXT("寄生體系"));
    R(TEXT("HandParasite"), TEXT("Parasite"), INVTEXT("天手族"), ERT::Yellow, INVTEXT("寄生於手部，分掌支（攻擊性力量）、臂支（變形或增強力量）、指支（輔助或生活性力量）。"));
    R(TEXT("HairParasite"), TEXT("Parasite"), INVTEXT("妖髮族"), ERT::Yellow, INVTEXT("寄生於頭髮，支配者能直接以頭髮代替雙手，或以頭髮束縛、攻擊，視實力可增加多隻手。"));
    R(TEXT("FaceParasite"), TEXT("Parasite"), INVTEXT("幼臉族"), ERT::Yellow, INVTEXT("寄生於臉部，支配者可自由變幻容貌（體型/聲音/性別/種族/氣息），擅幻術、精神攻擊。"));
    R(TEXT("EyeParasite"),  TEXT("Parasite"), INVTEXT("魔眼族"), ERT::Profound, INVTEXT("寄生於眼睛，具備最強種族天賦和最強精神力，按階級分普魔眼、次魔眼、至高魔眼。"));

    // ── 4. 天行體系（具魔黃→玄→地，接著不具魔黃→玄，照文件順序）─────────────
    Sys(TEXT("Sky"), INVTEXT("天行體系"));
    // 具魔：—黃階—
    R(TEXT("GhostCrow"), TEXT("Sky"), INVTEXT("詭鴉族"),     ERT::Yellow, INVTEXT("精通暗、冰屬性，擅使用魂力，能操控周遭亡魂。"));
    R(TEXT("WhiteDove"),  TEXT("Sky"), INVTEXT("白鴿族"),     ERT::Yellow, INVTEXT("精通精神力、光屬性魔力，經常擔任戰爭調停者，愛好和平，叫聲能撫平生靈情緒。"));
    R(TEXT("WaterDuck"),  TEXT("Sky"), INVTEXT("水鴨族"),     ERT::Yellow, INVTEXT("精通水、風屬性，精神力強大，肉體較弱，能在水面上高速移動。"));
    R(TEXT("Chicken"),    TEXT("Sky"), INVTEXT("只雞族"),     ERT::Yellow, INVTEXT("肉體、魔力、妖力都較弱小，但有完整靈智情感，常被各族當作食物，毫無心理壓力。"));
    R(TEXT("MagicBat"),   TEXT("Sky"), INVTEXT("魔蝠族"),     ERT::Yellow, INVTEXT("精通精神力、暗屬性魔力，多居住於洞穴，具備一些強大種族固有技。"));
    R(TEXT("Owl"),        TEXT("Sky"), INVTEXT("貓頭鷹族"),   ERT::Yellow, INVTEXT("魔力天賦高、智力高，有強大精神力感知，天生有窺探命運的本領，喜愛知識和研究。"));
    R(TEXT("WingedHumanoid"),TEXT("Sky"), INVTEXT("翼人族"),  ERT::Yellow, INVTEXT("背生白羽雙翅的種族，擁有強大的魔法與神聖力，實力普遍比普通人族強大（人、類人體系文件亦有交叉引用）。"));
    // 具魔：—玄階—
    R(TEXT("Peacock"),    TEXT("Sky"), INVTEXT("孔雀族"),     ERT::Profound, INVTEXT("魔力天賦高，精通元素屬性搭配，肉體較弱，能用「開屏」魅惑生靈進而支配其精神。"));
    R(TEXT("Parrot"),     TEXT("Sky"), INVTEXT("五彩鸚鵡族"), ERT::Profound, INVTEXT("魔力天賦高，精通元素屬性搭配，有強大學習能力，可快速模仿他人技術。"));
    R(TEXT("Pegasus"),    TEXT("Sky"), INVTEXT("天馬族"),     ERT::Profound, INVTEXT("雖為天行體系，但喜居住於瀑布旁，精通風、光、幻屬性。"));
    // 具魔：—地階—
    R(TEXT("GoldenRoc"),  TEXT("Sky"), INVTEXT("金鵬族"),     ERT::Earth, INVTEXT("飛行速度極快，精通金、風、光屬性。"));
    R(TEXT("Griffin"),    TEXT("Sky"), INVTEXT("獅鷲族"),     ERT::Earth, INVTEXT("肉體力量極強，精通金、風、火、光、雷等多重屬性。"));
    R(TEXT("SunCrow"),    TEXT("Sky"), INVTEXT("三足金烏族"), ERT::Earth, INVTEXT("體型龐大，行蹤神秘，精通極為強大之火屬性力量。"));
    // 不具魔：—黃階—
    R(TEXT("Eagle"),      TEXT("Sky"), INVTEXT("鷹族"),       ERT::Yellow, INVTEXT("真氣、妖力天賦高，多分佈於草原、沙漠、峽谷，部份會潛入人族領地抓走孩童。"));
    R(TEXT("SkyDemonKin"),TEXT("Sky"), INVTEXT("天行妖族"),   ERT::Yellow, INVTEXT("天行體系的妖族分支，主要獵食翼人族，也喜歡獵食類人體系。"));
    R(TEXT("RedWingSnake"),TEXT("Sky"),INVTEXT("赤翼蛇族"),   ERT::Yellow, INVTEXT("靈力、妖力天賦高，多分佈於叢林、沙漠、峽谷。"));
    R(TEXT("SwiftSwallow"),TEXT("Sky"),INVTEXT("疾燕族"),     ERT::Yellow, INVTEXT("速度極快，肉體強大，擅長途飛行。"));
    R(TEXT("Pelican"),    TEXT("Sky"), INVTEXT("鵜鶘族"),     ERT::Yellow, INVTEXT("肉體強大，擅長途飛行。"));
    R(TEXT("Seagull"),    TEXT("Sky"), INVTEXT("海鷗族"),     ERT::Yellow, INVTEXT("肉體強大，擅長途飛行。"));
    R(TEXT("JadeGoose"),  TEXT("Sky"), INVTEXT("碧鵝族"),     ERT::Yellow, INVTEXT("外貌優美、肉質鮮嫩可口，常被他族當作食物或奴隸，野外族群較為稀少。"));
    // 不具魔：—玄階—
    R(TEXT("Crane"),      TEXT("Sky"), INVTEXT("仙鶴族"),     ERT::Profound, INVTEXT("靈力和妖力天賦極高、外貌優美但肉體孱弱，按慣用能量分靈鶴和妖鶴。"));
    R(TEXT("WingedEagle"),TEXT("Sky"), INVTEXT("翼鵰族"),     ERT::Profound, INVTEXT("善用真氣、肉體極強，喜翱翔於天際，俯瞰大地。"));

    // ── 5. 肢節體系（黃階→玄階→地階，照文件順序）────────────────────────────
    Sys(TEXT("Limbed"), INVTEXT("肢節體系"));
    R(TEXT("BeetleKin"),       TEXT("Limbed"), INVTEXT("甲蟲族"),   ERT::Yellow,   INVTEXT("（敘述待補）"));
    R(TEXT("WarAnt"),          TEXT("Limbed"), INVTEXT("鬥蟻族"),   ERT::Yellow,   INVTEXT("（敘述待補）"));
    R(TEXT("LimbedDemonKin"),  TEXT("Limbed"), INVTEXT("肢節妖族"), ERT::Yellow,   INVTEXT("肢節體系的妖族分支（敘述待補）。"));
    R(TEXT("SickleMantis"), TEXT("Limbed"), INVTEXT("鐮刀螂族"), ERT::Yellow, INVTEXT("肉體強大，擅用真氣，極為敏捷，主要居住於森林，擅用一對大鐮刀揮砍攻擊。"));
    R(TEXT("HuntSpider"),   TEXT("Limbed"), INVTEXT("獵蛛族"),   ERT::Yellow, INVTEXT("肉體、妖力強大，擁有超高敏捷與靈活萬用的蜘蛛絲技能，主要居住於叢林。"));
    R(TEXT("DemonBee"),     TEXT("Limbed"), INVTEXT("魔蜂族"),   ERT::Yellow, INVTEXT("肉體、妖力強大，總群居行動，主食花蜜、精怪體系，較少與外界聯繫。"));
    R(TEXT("BloodMosquito"),TEXT("Limbed"), INVTEXT("血蚊族"),   ERT::Yellow, INVTEXT("蚊蟲吸食強大或大量精血後誕生靈智，體型巨大性情殘暴，居無定所，常受各族討伐。"));
    R(TEXT("DemonEyeFly"),  TEXT("Limbed"), INVTEXT("魔瞳蠅族"), ERT::Yellow, INVTEXT("擁有極強洞察力的複眼，帶有自己的「魔瞳」能力，嗅覺靈敏，喜歡屍體勝過活物。"));
    R(TEXT("EarthWorm"),    TEXT("Limbed"), INVTEXT("土蚯族"),   ERT::Yellow, INVTEXT("體型巨大，肉體強，居住於地底，會捕食除同族外任何體系的生靈。"));
    R(TEXT("BlackCentipede"),TEXT("Limbed"),INVTEXT("黑蜈蚣族"), ERT::Yellow, INVTEXT("肉體強、繁殖力強，魔力天賦高，擅毒、暗屬性，主要居住於沙漠、洞穴。"));
    R(TEXT("DemonScorpion"), TEXT("Limbed"), INVTEXT("魔蠍族"),   ERT::Yellow, INVTEXT("肉體強、繁殖力強，魔力天賦高，擅毒、暗屬性，個性陰險狡詐，注重外骨骼美感。"));
    R(TEXT("GiantRoach"),   TEXT("Limbed"), INVTEXT("巨翅蟑族"), ERT::Yellow, INVTEXT("生命力和繁殖力強，魔力充沛環境中蟑螂歷經死劫誕生靈智，擁有強大速度、體魄和再生力。"));
    R(TEXT("FantasyButterfly"),TEXT("Limbed"), INVTEXT("幻蝶族"),   ERT::Profound, INVTEXT("（敘述待補）"));
    R(TEXT("DreamSilkworm"),   TEXT("Limbed"), INVTEXT("夢蠶族"),   ERT::Profound, INVTEXT("（敘述待補）"));
    R(TEXT("BlackGoldCicada"), TEXT("Limbed"), INVTEXT("黑金魔蟬族"),ERT::Earth,   INVTEXT("（敘述待補）"));

    // ── 6. 水行體系（黃階→玄階→地階，照文件順序）────────────────────────────
    Sys(TEXT("Water"), INVTEXT("水行體系"));
    R(TEXT("SeaFish"),    TEXT("Water"), INVTEXT("海魚族"),   ERT::Yellow, INVTEXT("誕生靈智的強大魚種，有眾多支派。"));
    R(TEXT("DeepSeaFish"),TEXT("Water"), INVTEXT("深海魚族"), ERT::Yellow, INVTEXT("誕生靈智的強大深海魚種，肉體力量普遍強大，較為古老神秘，有眾多支派。"));
    R(TEXT("Crab"),       TEXT("Water"), INVTEXT("蟹族"),     ERT::Yellow, INVTEXT("誕生靈智的蟹，外殼堅硬、肉體強悍，擅使用兵器。"));
    R(TEXT("Shrimp"),     TEXT("Water"), INVTEXT("蝦族"),     ERT::Yellow, INVTEXT("誕生靈智的蝦，速度極快，擅操縱海底水流。"));
    R(TEXT("SoftClaw"),   TEXT("Water"), INVTEXT("軟爪族"),   ERT::Yellow, INVTEXT("包含海螺種、章魚種、魷魚種等多種支派，通常具強大妖力，各支派有特殊種族固有技。"));
    R(TEXT("ThornDemon"), TEXT("Water"), INVTEXT("魔棘族"),   ERT::Yellow, INVTEXT("包含海星種、海膽種、海參種等多種支派，通常具強大魔力，各支派有特殊種族固有技。"));
    R(TEXT("Cnidaria"),   TEXT("Water"), INVTEXT("刺胞族"),   ERT::Yellow, INVTEXT("包含水母種、海葵種、珊瑚種等多種支派，通常具強大妖力，各支派有特殊種族固有技。"));
    R(TEXT("Mermaid"),       TEXT("Water"), INVTEXT("人魚族"),   ERT::Yellow, INVTEXT("下半身為魚鰭的類人生物，生活於海中，普遍有較強魔法天賦，也擅用精神力和真氣；上半身皮膚細膩光滑，下半身魚鱗密布的魚尾肌肉密度極高，蘊含強大力量，使其在海中具備高機動性（人、類人體系文件亦有交叉引用）。"));
    R(TEXT("WaterDemonKin"), TEXT("Water"), INVTEXT("水行妖族"), ERT::Yellow, INVTEXT("水行體系的妖族分支，主要獵食人魚族。"));
    R(TEXT("Whale"),      TEXT("Water"), INVTEXT("巨鯨族"),   ERT::Profound, INVTEXT("主要居住於深海，性情平和、無欲無求，體內蘊藏無盡能量，多為魚龍族高階眷屬。"));
    R(TEXT("NineClaw"),   TEXT("Water"), INVTEXT("九爪族"),   ERT::Profound, INVTEXT("主要居住於深海，體型龐大，全身能高速再生，動作靈敏，又被稱作「深海惡爪」。"));
    R(TEXT("Mirage"),     TEXT("Water"), INVTEXT("蜃族"),     ERT::Profound, INVTEXT("主要居住於深海，能製造幻境迷惑獵物將其困死後捕食，智力偏高，鮮少露面。"));
    R(TEXT("DragonFish"), TEXT("Water"), INVTEXT("魚龍族"),   ERT::Earth, INVTEXT("擁有部份龍特徵的深海種族，地位僅次於巴哈姆特族，對大海種族的影響力最大。"));
    R(TEXT("Bahamut"),    TEXT("Water"), INVTEXT("巴哈姆特族"),ERT::Earth, INVTEXT("生活在海中的巨型魚龍種族，為大海主宰者。"));

    // ── 7. 半神體系（玄階→地階，照文件順序）──────────────────────────────────
    Sys(TEXT("SemiDivine"), INVTEXT("半神體系"));
    R(TEXT("ThousandWing"), TEXT("SemiDivine"), INVTEXT("千翼族"), ERT::Profound, INVTEXT("外型似人，背生千翼，擅空間法則，多分佈於聖域、高空雲層。"));
    R(TEXT("ThousandHand"), TEXT("SemiDivine"), INVTEXT("千手族"), ERT::Profound, INVTEXT("外型似人，體生千手，擅時間法則，多分佈於聖域。"));
    R(TEXT("Eternity"),     TEXT("SemiDivine"), INVTEXT("無常族"), ERT::Profound, INVTEXT("沒有固定的外型實體、顏色，魔力極為強大，擅以精神支配其它個體。"));
    R(TEXT("EvilRoot"),     TEXT("SemiDivine"), INVTEXT("邪根族"), ERT::Earth, INVTEXT("外型似克蘇魯，具備強大負面精神力，喜歡讓眾生陷入瘋狂，實為受克蘇魯污染後變異的生靈（邪靈體系文件亦有交叉引用）。"));
    R(TEXT("CelestialFox"), TEXT("SemiDivine"), INVTEXT("天狐族"), ERT::Earth, INVTEXT("又稱九尾狐族，擅幻術，可操縱部分因果、造化法則，能改變運勢，多分佈於森林、河川。"));
    R(TEXT("CelestialDog"), TEXT("SemiDivine"), INVTEXT("天狗族"), ERT::Earth, INVTEXT("善用肉體、靈力、真氣、妖力、神聖力，威嚴的種族，背翅可高速飛行，個體有少許神性。"));

    // ── 8. 萬獸體系（黃階→玄階→地階，照文件順序）────────────────────────────
    Sys(TEXT("Beast"), INVTEXT("萬獸體系"));
    R(TEXT("GhostClan"),    TEXT("Beast"), INVTEXT("鬼族"),   ERT::Yellow, INVTEXT("妖力天賦高，外型千奇百怪且多畸形噁心，為人族伴生種族，食人可進化外貌趨近人類，分紫、赤、青三大派系。"));
    R(TEXT("Goblin"),       TEXT("Beast"), INVTEXT("哥布林族"), ERT::Yellow, INVTEXT("淫慾貪念食慾極強，皮膚青綠色，繁殖力高、擅群居器具，以佈置陷阱埋伏劫掠路人著稱，個體弱但群體危害度極大。"));
    R(TEXT("Dog"),       TEXT("Beast"), INVTEXT("犬族"),   ERT::Yellow, INVTEXT("肉體力量強，靈力和妖力天賦高，多生活於叢林、草原、河川、峽谷，與人族較親近。"));
    R(TEXT("Cat"),       TEXT("Beast"), INVTEXT("貓族"),   ERT::Yellow, INVTEXT("肉體力量強，靈力和妖力天賦高，多生活於叢林、草原、河川、峽谷，部份喜奴役人類。"));
    R(TEXT("GhostRabbit"),  TEXT("Beast"), INVTEXT("鬼兔族"), ERT::Yellow, INVTEXT("食慾、破壞力、繁殖力強的暴虐種族，未開智時為森林災害，開智後可操縱未開智族群進行狩獵與破壞。"));
    R(TEXT("Beastman"),     TEXT("Beast"), INVTEXT("獸人族"), ERT::Yellow, INVTEXT("眾多分支外型接近類人，文明發展度高，肉體力量強，擅用真氣靈力，為戰鬥民族，注重實力與名譽。"));
    R(TEXT("Orc"),          TEXT("Beast"), INVTEXT("歐克族"), ERT::Yellow, INVTEXT("數量龐大、肉體強大，分綠、藍、黃三皮，大多貪欲淫慾食慾極強，普遍智力低下，能駕馭武器，會狩獵除同族外的一切動物。"));
    R(TEXT("BeastDemonKin"),TEXT("Beast"), INVTEXT("萬獸妖族"), ERT::Yellow, INVTEXT("萬獸體系的妖族分支，數量龐大、貪欲淫慾食慾極強，肉體力量強，擅用妖力真氣，普遍獵食各族尤其人族，故被多數種族排斥抵制。"));
    R(TEXT("Centaur"),   TEXT("Beast"), INVTEXT("半人馬族"),ERT::Yellow, INVTEXT("近似類人體系，有高度發展之文化，善使用真氣與弓箭，常利用占星術預測未來，多分佈於草原。"));
    R(TEXT("MadBull"),   TEXT("Beast"), INVTEXT("狂牛族"), ERT::Yellow, INVTEXT("肉體強，擅長使用真氣，能將力量積蓄於牛角並高速衝撞敵人，破壞力極強，多分佈於草原。"));
    R(TEXT("SacredElephant"),TEXT("Beast"),INVTEXT("神象族"),ERT::Yellow, INVTEXT("肉體極度強悍，體型大，長鼻子十分靈活，能製作並使用工具，但不擅使用能量。"));
    R(TEXT("WarSheep"),  TEXT("Beast"), INVTEXT("戰羊族"), ERT::Yellow, INVTEXT("真氣天賦極高，是萬獸體系中的智者，擅於謀略與經商，因肉質軟嫩鮮美而常被獵食。"));
    R(TEXT("Lizard"),    TEXT("Beast"), INVTEXT("蜥蜴族"), ERT::Yellow, INVTEXT("能以雙足站立，擅於使用真氣及各種兵器，也會進行建築、農耕、狩獵，自給自足。"));
    R(TEXT("ColorDeer"), TEXT("Beast"), INVTEXT("彩鹿族"), ERT::Yellow, INVTEXT("擅以妖術施展幻術，有強烈情感和藝術天份，模樣嬌小可愛，因珍貴材料遭大量獵殺圈養。"));
    R(TEXT("Dinosaur"),  TEXT("Beast"), INVTEXT("恐龍族"), ERT::Profound, INVTEXT("肉體極強，真氣、妖力天賦也高，多分佈於草原或叢林，有眾多分支。"));
    R(TEXT("Chimera"),   TEXT("Beast"), INVTEXT("奇美拉族"),ERT::Profound, INVTEXT("肉體極強，真氣、妖力天賦也高，個體差異極大，擁有神異固有技，喜歡掠食其它萬獸強化自身。"));
    R(TEXT("SkySnake"),  TEXT("Beast"), INVTEXT("天蛇族"), ERT::Profound, INVTEXT("善於使用肉體、靈力、真氣、妖力，文明發展度高，淫欲、食慾極強，經常侵略其它種族。"));
    R(TEXT("Pixiu"),     TEXT("Beast"), INVTEXT("貔貅族"), ERT::Earth, INVTEXT("擁有強大的命運和財富，與其為敵者將遭強烈厄運反噬，地位崇高，精通靈力與水屬性。"));
    R(TEXT("MoonToad"),  TEXT("Beast"), INVTEXT("月蟾族"), ERT::Earth, INVTEXT("可自由縮放體型，靈力與妖力天賦高，壽命極長，喜歡隱居在深山中，主要以肢節體系為食。"));

    // ── 9. 次元物種（玄階→地階，照文件順序）──────────────────────────────────
    Sys(TEXT("Dimensional"), INVTEXT("次元物種"));
    R(TEXT("Shadow"),       TEXT("Dimensional"), INVTEXT("影族"),   ERT::Profound, INVTEXT("具魔，為附體術創始族，附體術天賦高，居住在「影之世界」中。"));
    R(TEXT("FantasyBug"),   TEXT("Dimensional"), INVTEXT("幻想蟲族"),ERT::Profound, INVTEXT("具魔，「擬態」模式可大幅增強實力，能吞食其它種族而獲得其能力，繁殖力高。"));
    R(TEXT("MachineKin"),   TEXT("Dimensional"), INVTEXT("機械族"), ERT::Profound, INVTEXT("不具魔，力量以源能科技為主，居住於數位世界的機械之城，透過安裝程式零件變強，理論上能源充足可獲永恆生命。"));
    R(TEXT("ElementSpirit"),TEXT("Dimensional"), INVTEXT("元素靈族"),ERT::Profound, INVTEXT("具魔，靈智相對較低，體內蘊含大量元素能量，常被魔法使召喚獻祭以發動更強大元素魔法。"));
    R(TEXT("Mimic"),        TEXT("Dimensional"), INVTEXT("幻形族"), ERT::Earth, INVTEXT("具魔，能在一定程度上複製對手的能力和形象，居住在「鏡像世界」中。"));
    R(TEXT("Abyssal"),      TEXT("Dimensional"), INVTEXT("深淵族"), ERT::Earth, INVTEXT("具魔，擁有能污染、腐敗一切事物之力，以其它位面生靈為食，居住在「深淵」中。"));

    // ── 10. 神話體系（全天階，照文件出現順序）────────────────────────────────
    Sys(TEXT("Myth"), INVTEXT("神話體系"));
    R(TEXT("WingedDivineDragon"),    TEXT("Myth"), INVTEXT("翼神龍族"),  ERT::Heaven, INVTEXT("肉體、魔法、妖力、精神力俱強，壽命悠久，法體雙修最強種族，約十分之一可用靈力，大多數個體能成神。"));
    R(TEXT("AuspiciousDivineDragon"),TEXT("Myth"), INVTEXT("瑞神龍族"),  ERT::Heaven, INVTEXT("肉體、靈力、真氣、神聖力、精神力俱強，壽命悠久，仙體雙修最強種族，無法使用魔法但仙道靈力極度強大，可支配法則。"));
    R(TEXT("Phoenix"),               TEXT("Myth"), INVTEXT("鳳族"),    ERT::Heaven, INVTEXT("分神鳳、魔鳳兩支，各自將單一屬性力量發揮至極致，約十分之一可同時擁有靈力與魔力並使用䵻龗，瀕死時能涅槃重生獲得強大力量。"));
    R(TEXT("Giant"),                 TEXT("Myth"), INVTEXT("巨人族"),  ERT::Heaven, INVTEXT("（敘述待補）"));
    R(TEXT("Titan"),                 TEXT("Myth"), INVTEXT("泰坦族"),  ERT::Heaven, INVTEXT("（敘述待補）"));
    R(TEXT("Qilin"),                 TEXT("Myth"), INVTEXT("麒麟族"),  ERT::Heaven, INVTEXT("（敘述待補）"));
    R(TEXT("SacredTurtle"),  TEXT("Myth"), INVTEXT("神龜族"),   ERT::Heaven, INVTEXT("體型如島嶼般龐大，壽命無盡的神龜，能一定程度掌握天機。"));
    R(TEXT("FeatherSerpent"),TEXT("Myth"), INVTEXT("羽蛇神族"), ERT::Heaven, INVTEXT("能一定程度控制萬物的興衰，並賦予生靈「生命」和「死亡」。"));
    R(TEXT("FourGods"),      TEXT("Myth"), INVTEXT("四神族"),   ERT::Heaven, INVTEXT("有青龍、白虎、朱雀、玄武四大支派。"));
    R(TEXT("FourEvils"),     TEXT("Myth"), INVTEXT("四凶族"),   ERT::Heaven, INVTEXT("有混沌、饕餮、窮奇、檮杌四大支派。"));
    R(TEXT("CelestialSphere"),TEXT("Myth"),INVTEXT("天球族"),  ERT::Heaven, INVTEXT("萬法根源元素的創造者，擁有最強魔力天賦，外型為漂浮空中的球體，自詡為神，魔法極度強大但肉身脆弱，無法使用魔力以外的能量。"));
    R(TEXT("Dreamer"),       TEXT("Myth"), INVTEXT("夢幻族"),  ERT::Heaven, INVTEXT("可依想像力改寫現實法則，是蒼究世界種族血脈力量最強的種族，自我唯心狀態全憑想像力決定。"));
    R(TEXT("Behemoth"),      TEXT("Myth"), INVTEXT("比蒙族"),   ERT::Heaven, INVTEXT("破壞力最強的凡間種族，可輕鬆駕馭各種能量，達到最佳破壞效果。"));
    R(TEXT("Leviathan"),     TEXT("Myth"), INVTEXT("利維坦族"), ERT::Heaven, INVTEXT("居住於海底，型態為巨型海蛇，支配著深海的一切。"));
    R(TEXT("Cthulhu"),       TEXT("Myth"), INVTEXT("克蘇魯族"),ERT::Heaven, INVTEXT("（敘述待補）"));

    // ── 11. 邪靈體系（黃階→玄階→地階，照文件順序）───────────────────────────
    Sys(TEXT("Evil"), INVTEXT("邪靈體系"));
    R(TEXT("Eerie"),      TEXT("Evil"), INVTEXT("詭異族"), ERT::Yellow,   INVTEXT("（敘述待補，文件僅標註其分類為「虛偽種」）"));
    R(TEXT("Vampire"),    TEXT("Evil"), INVTEXT("血族"),   ERT::Profound, INVTEXT("外觀近似人族，依血統濃度而有不同程度特徵，擁有強大魔力、妖力，肉身強度、再生力、壽命、智慧與學習能力也極強，擅長暗屬性，魔力親和力強。"));
    R(TEXT("Yaksha"),     TEXT("Evil"), INVTEXT("夜叉族"),   ERT::Profound, INVTEXT("肉體力量極強，善用妖力、兵器，性格嗜血暴虐，喜獵食異族。"));
    R(TEXT("Lich"),       TEXT("Evil"), INVTEXT("巫妖族"),   ERT::Profound, INVTEXT("擁有強大魔力、妖力，可一定程度「掌控」魔法，有「召喚冥界生靈」的種族固有技。"));
    // 邪根族（文件原文位置在巫妖族之後）兼具半神/邪靈體系特質，註冊在 SemiDivine 體系下，
    // 見該區塊——文件描述本身就是「同為半神體系與邪靈體系之生靈」，兩邊都合理，這裡選
    // SemiDivine 是因為已在那裡緊接著天狐族/天狗族（地階）的位置插入，順序更貼合原文。
    R(TEXT("NightDemon"), TEXT("Evil"), INVTEXT("暗夜妖族"), ERT::Earth, INVTEXT("怨念和負面情緒的結晶體，肉體強大，擁有最強妖力，為暗屬性半靈體。"));
    R(TEXT("SeaDemon"),   TEXT("Evil"), INVTEXT("海魔族"),   ERT::Earth, INVTEXT("深海中最強種族，肉體強大，妖力、魔力、靈力天賦極強，有眾多分支。"));
    R(TEXT("Gluttony"),   TEXT("Evil"), INVTEXT("暴食族"),   ERT::Earth, INVTEXT("形似史萊姆的邪惡污穢集合體，藉由吞噬生物讓自己變強，妖力強大而詭譎。"));

    // ── 12. 聖靈體系（黃階→玄階→地階，照文件順序）───────────────────────────
    Sys(TEXT("Holy"), INVTEXT("聖靈體系"));
    R(TEXT("ShineSpirit"), TEXT("Holy"), INVTEXT("耀精族"),   ERT::Yellow, INVTEXT("軀體為一顆飛行眼球，兩側各長三根觸手，天生具高速飛行與短距瞬移能力，善空間魔法與幻術。"));
    R(TEXT("LanternEvil"), TEXT("Holy"), INVTEXT("幻靈惡族"), ERT::Yellow, INVTEXT("形似燈籠的半透明生靈，天生具隱身能力，能吸收能量化為己用，性格頑劣喜屠殺異族。"));
    R(TEXT("Unicorn"),     TEXT("Holy"), INVTEXT("獨角族"),   ERT::Profound, INVTEXT("通體晶瑩潔白、長有獨角和雙翼，智力性格俱佳，能釋放元素魔法，目光有治癒效果。"));
    R(TEXT("SongSpirit"),  TEXT("Holy"), INVTEXT("歌訟族"),   ERT::Profound, INVTEXT("外形似人但通體潔白柔軟，不具戰鬥能力，喜愛歌唱，因肉質鮮美遭大肆捕獵幾近絕跡。"));
    R(TEXT("FalseJustice"),TEXT("Holy"), INVTEXT("超負義族"), ERT::Earth, INVTEXT("金色巨型鳥喙造型，能透過信仰自己獲得神聖力，自稱「超正義族」實則以正義之名掠奪異族。"));

    // ── 13. 靈界種族（天使/惡魔/精靈/英靈，暫標天階，詳見計畫文件灰色地帶說明） ──
    Sys(TEXT("SpiritRealm"), INVTEXT("靈界種族"));
    R(TEXT("Angel"),  TEXT("SpiritRealm"), INVTEXT("天使"), ERT::Heaven, INVTEXT("光明與純潔之魂，根源乃七美德的化身，每種美德是一種職位，依附強大權能，可相互爭奪。"));
    R(TEXT("Demon"),  TEXT("SpiritRealm"), INVTEXT("惡魔"), ERT::Heaven, INVTEXT("黑暗與渾濁之魂，根源乃七大罪的化身，每種大罪是一種職位，依附強大權能，可相互爭奪。"));
    R(TEXT("Elf"),    TEXT("SpiritRealm"), INVTEXT("精靈"), ERT::Heaven, INVTEXT("自然與法則之魂，根源乃自然力量的化身，有十二大元素支派，每種元素精靈王依附強大權能。"));
    R(TEXT("Heroic"), TEXT("SpiritRealm"), INVTEXT("英靈"), ERT::Heaven, INVTEXT("部分神話/歷史/故事人物，因生靈意念虛構、死後事蹟傳頌、或價值達神格邊緣而選擇成為英靈。"));

    // ── 14. 冥界種族（原文用「1階/2階」，2階對應 Profound） ───────────────────
    Sys(TEXT("Underworld"), INVTEXT("冥界種族"));
    R(TEXT("Skeleton"),  TEXT("Underworld"), INVTEXT("骷髏"),     ERT::Yellow, INVTEXT("以魂力驅動、或靈魂回歸的骸骨，若保留靈智可透過修煉魂力提升實力。"));
    R(TEXT("Zombie"),    TEXT("Underworld"), INVTEXT("殭屍"),     ERT::Yellow, INVTEXT("受殭屍病毒感染或怨念極深的屍體，以同族血肉為食，智力低下，進食越多越能恢復靈智。"));
    R(TEXT("Mummy"),     TEXT("Underworld"), INVTEXT("木乃伊"),   ERT::Yellow, INVTEXT("以特殊方式下葬的死者，死後被詛咒的魂魄仍控制著軀體，具備相當強的魂力。"));
    R(TEXT("Ghost"),     TEXT("Underworld"), INVTEXT("幽靈"),     ERT::Yellow, INVTEXT("生靈死亡後到轉世或回歸天地前的過渡狀態。"));
    R(TEXT("GhoulThing"),TEXT("Underworld"), INVTEXT("鬼物"),     ERT::Yellow, INVTEXT("凡間或冥界怨念深重的幽靈，形態不定，按實力分級為「整、亂、奇、域、淵」。"));
    R(TEXT("Cerberus"),  TEXT("Underworld"), INVTEXT("克爾柏洛斯"),ERT::Yellow, INVTEXT("又稱地獄犬，負責看守冥界入口。"));
    R(TEXT("Undead"),    TEXT("Underworld"), INVTEXT("亡靈"),     ERT::Profound, INVTEXT("奇以上之幽靈蛻變而成，魔力和魂力天賦極強，可吞噬異火、詭氣、奇冰維持自身存在。"));
    R(TEXT("Ghoul"),     TEXT("Underworld"), INVTEXT("鬼怪"),     ERT::Profound, INVTEXT("妖力、魂力、靈力都極強，還具有受肉後肉身強大的種族特性。"));
    R(TEXT("DeathGod"),  TEXT("Underworld"), INVTEXT("死神"),     ERT::Profound, INVTEXT("亡靈或幽靈透過修煉魂術、考核、魂灌接受死神之力而成，具備專屬死神鐮刀。"));

    // ── 15. 人造物種：內容極少，三族都只有名稱 ──────────────────────────────
    Sys(TEXT("Artificial"), INVTEXT("人造物種"));
    R(TEXT("AnimatedComposite"), TEXT("Artificial"), INVTEXT("活化複合生體"), ERT::Yellow, INVTEXT("（敘述待補，文件僅有角色案例「銳一森」，無泛種族描述）"));
    R(TEXT("CloudSlave"),        TEXT("Artificial"), INVTEXT("雲奴"),       ERT::Yellow, INVTEXT("（敘述待補）"));
    R(TEXT("CorpseSlave"),       TEXT("Artificial"), INVTEXT("屍奴"),       ERT::Yellow, INVTEXT("（敘述待補）"));

    // 神界種族：依文件內容判斷為飛升後神格狀態，不是可創建種族，故不註冊體系也不註冊種族
    // （見 RaceRegistry.h 頂部說明）。
}

const FRaceRegistry::FData& FRaceRegistry::GetData()
{
    static FData Data;
    if (Data.Systems.IsEmpty()) Init(Data.Systems, Data.Races);
    return Data;
}

const TArray<FRaceSystemDefinition>& FRaceRegistry::AllSystems()
{
    return GetData().Systems;
}

TArray<const FRaceDefinition*> FRaceRegistry::RacesInSystem(FName SystemId)
{
    TArray<const FRaceDefinition*> Out;
    for (const TPair<FName, FRaceDefinition>& Pair : GetData().Races)
        if (Pair.Value.SystemId == SystemId)
            Out.Add(&Pair.Value);
    return Out;
}

const FRaceDefinition* FRaceRegistry::Find(FName RaceId)
{
    return GetData().Races.Find(RaceId);
}

int32 FRaceRegistry::CostForTier(ERaceTier Tier)
{
    switch (Tier)
    {
        case ERaceTier::Yellow:   return 0;
        case ERaceTier::Profound: return 10;
        case ERaceTier::Earth:    return 50;
        case ERaceTier::Heaven:   return 100;
        default:                 return 0;
    }
}
