// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Engine/UserDefinedStruct.h"
#include "Kismet2/ListenerManager.h"

struct FEdGraphPinType;
struct FStructVariableDescription;
class UBlueprint;
class UUserDefinedStruct;

class UNREALED_API FStructureEditorUtils
{
public:
	enum EStructureEditorChangeInfo
	{
		Unknown,
		AddedVariable,
		RemovedVariable,
		RenamedVariable,
		VariableTypeChanged,
		MovedVariable,
		DefaultValueChanged,
	};

	class FStructEditorManager : public FListenerManager<UUserDefinedStruct, EStructureEditorChangeInfo>
	{
		FStructEditorManager() {}
	public:
		UNREALED_API static FStructEditorManager& Get();

		class UNREALED_API ListenerType : public InnerListenerType<FStructEditorManager>
		{
		};

		/** The current reason why a structure is being updated */
		UNREALED_API static EStructureEditorChangeInfo ActiveChange;
	};

	typedef FStructEditorManager::ListenerType INotifyOnStructChanged;

	template<class TElement>
	struct FFindByNameHelper
	{
		const FName Name;

		FFindByNameHelper(FName InName) : Name(InName) { }

		bool operator() (const TElement& Element) const
		{
			return (Name == Element.VarName);
		}
	};

	template<class TElement>
	struct FFindByGuidHelper
	{
		const FGuid Guid;

		FFindByGuidHelper(FGuid InGuid) : Guid(InGuid) { }

		bool operator() (const TElement& Element) const
		{
			return (Guid == Element.VarGuid);
		}
	};

	//STRUCTURE
	static UUserDefinedStruct* CreateUserDefinedStruct(UObject* InParent, FName Name, EObjectFlags Flags);

	static void CompileStructure(UUserDefinedStruct* Struct);

	static FString GetTooltip(const UUserDefinedStruct* Struct);

	static bool ChangeTooltip(UUserDefinedStruct* Struct, const FString& InTooltip);

	//VARIABLE
	static bool AddVariable(UUserDefinedStruct* Struct, const FEdGraphPinType& VarType);

	static bool RemoveVariable(UUserDefinedStruct* Struct, FGuid VarGuid);

	static bool RenameVariable(UUserDefinedStruct* Struct, FGuid VarGuid, const FString& NewDisplayNameStr);

	static bool RenameVariable(UUserDefinedStruct* Struct, const FString& OldDisplayNameStr, const FString& NewDisplayNameStr);

	static bool ChangeVariableType(UUserDefinedStruct* Struct, FGuid VarGuid, const FEdGraphPinType& NewType);

	static bool ChangeVariableDefaultValue(UUserDefinedStruct* Struct, FGuid VarGuid, const FString& NewDefaultValue);

	static bool IsUniqueVariableFriendlyName(const UUserDefinedStruct* Struct, const FString& DisplayName);

	static FString GetVariableFriendlyName(const UUserDefinedStruct* Struct, FGuid VarGuid);

	static FString GetVariableFriendlyNameForProperty(const UUserDefinedStruct* Struct, const FProperty* Property);

	static FProperty* GetPropertyByFriendlyName(const UUserDefinedStruct* Struct, FString DisplayName);

	static FString GetVariableTooltip(const UUserDefinedStruct* Struct, FGuid VarGuid);
	
	static bool ChangeVariableTooltip(UUserDefinedStruct* Struct, FGuid VarGuid, const FString& InTooltip);

	static bool ChangeEditableOnBPInstance(UUserDefinedStruct* Struct, FGuid VarGuid, bool bInIsEditable);

	static bool ChangeSaveGameEnabled(UUserDefinedStruct* Struct, FGuid VarGuid, bool bInSaveGame);

	enum EMovePosition
	{
		PositionAbove,
		PositionBelow,
	};

	/**
	 * Move the variable with the given guid in the struct to be immediately above or below another variable.
	 * 
	 * @param Struct         The struct containing the variable to move.
	 * @param MoveVarGuid    The guid of the variable being moved.
	 * @param RelativeToGuid The guid of the variable above/below which MoveVarGuid is being moved.
	 * @param Position       Whether to put MoveVarGuid above or below RelativeToGuid.
	 * 
	 * @return Whether the variable was actually moved.
	 */
	static bool MoveVariable(UUserDefinedStruct* Struct, FGuid MoveVarGuid, FGuid RelativeToGuid, EMovePosition Position);

	/** Checks whether MoveVariable can actually move the variable with the given guid. */
	static bool CanMoveVariable(UUserDefinedStruct* Struct, FGuid MoveVarGuid, FGuid RelativeToGuid, EMovePosition Position);

	//Multi-line text
	static bool CanEnableMultiLineText(const UUserDefinedStruct* Struct, FGuid VarGuid);

	static bool ChangeMultiLineTextEnabled(UUserDefinedStruct* Struct, FGuid VarGuid, bool bIsEnabled);

	static bool IsMultiLineTextEnabled(const UUserDefinedStruct* Struct, FGuid VarGuid);

	//3D Widget
	static bool CanEnable3dWidget(const UUserDefinedStruct* Struct, FGuid VarGuid);

	static bool Change3dWidgetEnabled(UUserDefinedStruct* Struct, FGuid VarGuid, bool bIsEnabled);

	static bool Is3dWidgetEnabled(const UUserDefinedStruct* Struct, FGuid VarGuid);

	//GUID AND VAR DESC
	static TArray<FStructVariableDescription>& GetVarDesc(UUserDefinedStruct* Struct);

	static const TArray<FStructVariableDescription>& GetVarDesc(const UUserDefinedStruct* Struct);

	static TArray<FStructVariableDescription>* GetVarDescPtr(UUserDefinedStruct* Struct);

	static const TArray<FStructVariableDescription>* GetVarDescPtr(const UUserDefinedStruct* Struct);

	static FStructVariableDescription* GetVarDescByGuid(UUserDefinedStruct* Struct, FGuid VarGuid);

	static const FStructVariableDescription* GetVarDescByGuid(const UUserDefinedStruct* Struct, FGuid VarGuid);

	static FGuid GetGuidForProperty(const FProperty* Property);

	static FProperty* GetPropertyByGuid(const UUserDefinedStruct* Struct, FGuid VarGuid);

	static FGuid GetGuidFromPropertyName(FName Name);

	//MISC
	static void ModifyStructData(UUserDefinedStruct* Struct);

	static bool UserDefinedStructEnabled();

	static void RemoveInvalidStructureMemberVariableFromBlueprint(UBlueprint* Blueprint);

	//DEFAULT VALUE
	static void RecreateDefaultInstanceInEditorData(UUserDefinedStruct* Struct);

	//VALIDATION
	static bool CanHaveAMemberVariableOfType(const UUserDefinedStruct* Struct, const FEdGraphPinType& VarType, FString* OutMsg = NULL);

	enum EStructureError
	{
		Ok, 
		Recursion,
		FallbackStruct,
		NotCompiled,
		NotBlueprintType,
		NotSupportedType,
		EmptyStructure
	};

	/** Can the structure be a member variable for a BPGClass or BPGStruct */
	static EStructureError IsStructureValid(const UScriptStruct* Struct, const UStruct* RecursionParent = NULL, FString* OutMsg = NULL);

	/** called after UDS was changed by editor*/
	static void OnStructureChanged(UUserDefinedStruct* Struct, EStructureEditorChangeInfo ChangeReason = EStructureEditorChangeInfo::Unknown);

	static void BroadcastPreChange(UUserDefinedStruct* Struct);
	static void BroadcastPostChange(UUserDefinedStruct* Struct);
};

