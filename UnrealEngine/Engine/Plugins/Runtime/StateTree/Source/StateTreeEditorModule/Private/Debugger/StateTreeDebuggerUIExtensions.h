// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointerFwd.h"

struct FStateTreeEditorNode;
enum class EStateTreeConditionEvaluationMode : uint8;

class SWidget;
class IPropertyHandle;
class IDetailLayoutBuilder;
class FDetailWidgetRow;
class FStateTreeViewModel;
class UStateTreeEditorData;

namespace UE::StateTreeEditor::DebuggerExtensions
{

TSharedRef<SWidget> CreateStateWidget(const IDetailLayoutBuilder& DetailBuilder, UStateTreeEditorData* TreeData);
TSharedRef<SWidget> CreateEditorNodeWidget(const TSharedPtr<IPropertyHandle>& StructPropertyHandle, UStateTreeEditorData* TreeData);
TSharedRef<SWidget> CreateTransitionWidget(const TSharedPtr<IPropertyHandle>& StructPropertyHandle, UStateTreeEditorData* TreeData);

}; // UE::StateTreeEditor::DebuggerExtensions
