#include "NPCBrainSettings.h"

UNPCBrainSettings::UNPCBrainSettings()
{
	CategoryName = TEXT("SkillCreator");
	SectionName  = TEXT("NPCBrain");

	WorldviewSystemPrompt = TEXT(
		"你正在為一個名為「蒼究」的奇幻世界生成 NPC 身分設定。"
		"這個世界存在境界修煉體系、多元種族與勢力。"
		"請以 JSON 格式回覆一個新 NPC 的身分，欄位包含："
		"Name（姓名）、Species（種族）、Faction（所屬勢力）、Role（身分/職業）、"
		"Traits（性格特質陣列）、SpeechStyle（說話風格）、Backstory（簡短背景故事，2-3句）。"
		"只回傳 JSON，不要其他文字。");
}
