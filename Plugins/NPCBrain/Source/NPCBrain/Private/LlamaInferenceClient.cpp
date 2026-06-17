#include "LlamaInferenceClient.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

void FLlamaInferenceClient::Configure(const FString& InBaseUrl)
{
	BaseUrl = InBaseUrl;
}

FString FLlamaInferenceClient::RoleToString(ENPCMessageRole Role)
{
	switch (Role)
	{
		case ENPCMessageRole::System:    return TEXT("system");
		case ENPCMessageRole::Assistant: return TEXT("assistant");
		default:                          return TEXT("user");
	}
}

void FLlamaInferenceClient::SendMessages(
	const TArray<FNPCMessage>& Messages,
	FOnInferenceComplete       OnComplete,
	FOnInferenceError          OnError,
	float Temperature,
	int32 MaxTokens)
{
	if (BaseUrl.IsEmpty())
	{
		OnError.ExecuteIfBound(0, TEXT("NPCBrain: client not configured"));
		return;
	}

	// Build JSON request body
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("model"), TEXT("local"));

	TArray<TSharedPtr<FJsonValue>> MsgArray;
	for (const FNPCMessage& Msg : Messages)
	{
		TSharedPtr<FJsonObject> MsgObj = MakeShared<FJsonObject>();
		MsgObj->SetStringField(TEXT("role"),    RoleToString(Msg.Role));
		MsgObj->SetStringField(TEXT("content"), Msg.Content);
		MsgArray.Add(MakeShared<FJsonValueObject>(MsgObj));
	}
	Root->SetArrayField(TEXT("messages"),    MsgArray);
	Root->SetNumberField(TEXT("temperature"), static_cast<double>(Temperature));
	Root->SetNumberField(TEXT("max_tokens"),  static_cast<double>(MaxTokens));
	Root->SetBoolField  (TEXT("stream"),      false);

	FString BodyStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

	// HTTP request — TDelegate is move-only, so capture by move into lambda
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req =
		FHttpModule::Get().CreateRequest();
	Req->SetURL(BaseUrl + TEXT("/v1/chat/completions"));
	Req->SetVerb(TEXT("POST"));
	Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Req->SetContentAsString(BodyStr);
	Req->SetTimeout(120.0f);

	Req->OnProcessRequestComplete().BindLambda(
		[OnComplete = MoveTemp(OnComplete), OnError = MoveTemp(OnError)]
		(FHttpRequestPtr, FHttpResponsePtr Resp, bool bOK) mutable
		{
			if (!bOK || !Resp)
			{
				OnError.ExecuteIfBound(0, TEXT("NPCBrain: connection failed"));
				return;
			}

			const int32 Code = Resp->GetResponseCode();
			if (Code != 200)
			{
				OnError.ExecuteIfBound(Code, Resp->GetContentAsString());
				return;
			}

			TSharedPtr<FJsonObject> RespJson;
			TSharedRef<TJsonReader<>> Reader =
				TJsonReaderFactory<>::Create(Resp->GetContentAsString());

			if (!FJsonSerializer::Deserialize(Reader, RespJson) || !RespJson)
			{
				OnError.ExecuteIfBound(500, TEXT("NPCBrain: failed to parse response JSON"));
				return;
			}

			const TArray<TSharedPtr<FJsonValue>>* Choices = nullptr;
			if (!RespJson->TryGetArrayField(TEXT("choices"), Choices) ||
			    !Choices || Choices->Num() == 0)
			{
				OnError.ExecuteIfBound(500, TEXT("NPCBrain: no choices in response"));
				return;
			}

			const TSharedPtr<FJsonObject>* ChoiceObj = nullptr;
			if (!(*Choices)[0]->TryGetObject(ChoiceObj) || !ChoiceObj)
			{
				OnError.ExecuteIfBound(500, TEXT("NPCBrain: malformed choice"));
				return;
			}

			const TSharedPtr<FJsonObject>* MsgObj = nullptr;
			if (!(*ChoiceObj)->TryGetObjectField(TEXT("message"), MsgObj) || !MsgObj)
			{
				OnError.ExecuteIfBound(500, TEXT("NPCBrain: no message in choice"));
				return;
			}

			FString Content;
			(*MsgObj)->TryGetStringField(TEXT("content"), Content);
			OnComplete.ExecuteIfBound(Content);
		});

	Req->ProcessRequest();
}
