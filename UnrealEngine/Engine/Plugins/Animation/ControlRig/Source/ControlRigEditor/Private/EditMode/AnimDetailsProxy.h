// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "Engine/EngineBaseTypes.h"
#include "ControlRigControlsProxy.h"
#include "AnimDetailsProxy.generated.h"


/**
* State for details, including if they are selected(shown in curve editor) or have multiple different values.
*/
struct FAnimDetailPropertyState
{
	bool bMultiple = false;
	bool bSelected = false;
};

struct FAnimDetailVectorState
{
	bool bXMultiple = false;
	bool bYMultiple = false;
	bool bZMultiple = false;

	bool bXSelected = false;
	bool bYSelected = false;
	bool bZSelected = false;
};

//note control rig uses 'float' controls so we call this float though it's a 
//double internally so we can use same for non-control rig parameters
USTRUCT(BlueprintType)
struct FAnimDetailProxyFloat
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Interp, Category = "Float", meta = (SliderExponent = "1.0"))
	double Float = 0.0;

};

USTRUCT(BlueprintType)
struct FAnimDetailProxyBool
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Interp, Category = "Bool")
	bool Bool = false;

};

USTRUCT(BlueprintType)
struct FControlRigEnumControlProxyValue
{
	GENERATED_USTRUCT_BODY()

	FControlRigEnumControlProxyValue()
	{
		EnumType = nullptr;
		EnumIndex = INDEX_NONE;
	}

	UPROPERTY()
	TObjectPtr<UEnum> EnumType;

	UPROPERTY(EditAnywhere, Category = Enum)
	int32 EnumIndex;
};

class FControlRigEnumControlProxyValueDetails : public IPropertyTypeCustomization
{
public:

	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FControlRigEnumControlProxyValueDetails);
	}

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

private:

	int32 GetEnumValue() const;
	void OnEnumValueChanged(int32 InValue, ESelectInfo::Type InSelectInfo, TSharedRef<IPropertyHandle> InStructHandle);

	UAnimDetailControlsProxyEnum* ProxyBeingCustomized;
};

USTRUCT(BlueprintType)
struct FAnimDetailProxyInteger
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Interp, Category = "Integer")
	int64 Integer = 0;

};

USTRUCT(BlueprintType)
struct FAnimDetailProxyVector3 
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Interp, Category = "Vector")
	double X = 0.0;

	UPROPERTY(EditAnywhere, Interp, Category = "Vector")
	double Y = 0.0;

	UPROPERTY(EditAnywhere, Interp, Category = "Vector")
	double Z = 0.0;

	FVector ToVector() const { return FVector(X, Y, Z); }
};

USTRUCT(BlueprintType)
struct  FAnimDetailProxyLocation 
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Interp, Category = "Location", meta = (Delta = "0.5", SliderExponent = "1", LinearDeltaSensitivity = "1"))
	double LX = 0.0;

	UPROPERTY(EditAnywhere, Interp, Category = "Location", meta = (Delta = "0.5", SliderExponent = "1", LinearDeltaSensitivity = "1"))
	double LY = 0.0;

	UPROPERTY(EditAnywhere, Interp, Category = "Location", meta = (Delta = "0.5", SliderExponent = "1", LinearDeltaSensitivity = "1"))
	double LZ = 0.0;

	FAnimDetailVectorState State;

	FAnimDetailProxyLocation() : LX(0.0), LY(0.0), LZ(0.0) {};
	FAnimDetailProxyLocation(const FVector& InVector, const FAnimDetailVectorState &InState) 
	{
		LX = InVector.X; LY = InVector.Y; LZ = InVector.Z; State = InState;
	}
	FAnimDetailProxyLocation(const FVector3f& InVector, const FAnimDetailVectorState& InState) 
	{
		LX = InVector.X; LY = InVector.Y; LZ = InVector.Z; State = InState;
	}
	FVector ToVector() const { return FVector(LX, LY, LZ); }
	FVector3f ToVector3f() const { return FVector3f(LX, LY, LZ); }

};

USTRUCT(BlueprintType)
struct  FAnimDetailProxyRotation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Interp, Category = "Rotation", meta = (Delta = "0.5", SliderExponent = "1", LinearDeltaSensitivity = "1"))
	double RX = 0.0;

	UPROPERTY(EditAnywhere, Interp, Category = "Rotation", meta = (Delta = "0.5", SliderExponent = "1", LinearDeltaSensitivity = "1"))
	double RY = 0.0;

	UPROPERTY(EditAnywhere, Interp, Category = "Rotation", meta = (Delta = "0.5", SliderExponent = "1", LinearDeltaSensitivity = "1"))
	double RZ = 0.0;

	FAnimDetailVectorState State;

	FAnimDetailProxyRotation() : RX(0.0), RY(0.0), RZ(0.0) {};
	FAnimDetailProxyRotation(const FRotator& InRotator, const FAnimDetailVectorState& InState) { FromRotator(InRotator); State = InState; }
	FAnimDetailProxyRotation(const FVector3f& InVector, const FAnimDetailVectorState& InState) 
	{ 
		RX = InVector.X; RY = InVector.Y; RZ = InVector.Z; State = InState;
	}

	FVector ToVector() const { return FVector(RX, RY, RZ); }
	FVector3f ToVector3f() const { return FVector3f(RX, RY, RZ); }
	FRotator ToRotator() const { FRotator Rot; Rot = Rot.MakeFromEuler(ToVector()); return Rot; }
	void FromRotator(const FRotator& InRotator) { FVector Vec(InRotator.Euler()); RX = Vec.X; RY = Vec.Y; RZ = Vec.Z; }
};

USTRUCT(BlueprintType)
struct  FAnimDetailProxyScale 
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Interp, Category = "Scale", meta = (Delta = "0.5", SliderExponent = "1", LinearDeltaSensitivity = "1"))
	double SX = 1.0;

	UPROPERTY(EditAnywhere, Interp, Category = "Scale", meta = (Delta = "0.5", SliderExponent = "1", LinearDeltaSensitivity = "1"))
	double SY = 1.0;

	UPROPERTY(EditAnywhere, Interp, Category = "Scale", meta = (Delta = "0.5", SliderExponent = "1", LinearDeltaSensitivity = "1"))
	double SZ = 1.0;

	FAnimDetailVectorState State;

	FAnimDetailProxyScale() : SX(1.0), SY(1.0), SZ(1.0) {};
	FAnimDetailProxyScale(const FVector& InVector, const FAnimDetailVectorState& InState) 
	{
		SX = InVector.X; SY = InVector.Y; SZ = InVector.Z; State = InState;
	}
	FAnimDetailProxyScale(const FVector3f& InVector, const FAnimDetailVectorState& InState)
	{
		SX = InVector.X; SY = InVector.Y; SZ = InVector.Z; State = InState;
	}
	FVector ToVector() const { return FVector(SX, SY, SZ); }
	FVector3f ToVector3f() const { return FVector3f(SX, SX, SZ); }
};

USTRUCT(BlueprintType)
struct  FAnimDetailProxyVector2D
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Interp, Category = "Vector2D", meta = (Delta = "0.05", SliderExponent = "1", LinearDeltaSensitivity = "1"))
	double X = 0.0;

	UPROPERTY(EditAnywhere, Interp, Category = "Vector2D", meta = (Delta = "0.05", SliderExponent = "1", LinearDeltaSensitivity = "1"))
	double Y = 0.0;


	FAnimDetailVectorState State;

	FAnimDetailProxyVector2D() : X(0.0), Y(0.0) {};
	FAnimDetailProxyVector2D(const FVector2D& InVector, const FAnimDetailVectorState& InState) 
	{ 
		X = InVector.X; Y = InVector.Y; State = InState;

	}
	FVector2D ToVector2D() const { return FVector2D(X,Y); }
};

UCLASS()
class UAnimDetailControlsKeyedProxy : public UControlRigControlsProxy
{
	GENERATED_BODY()
public:
	virtual void SetKey(TSharedPtr<ISequencer>& Sequencer, const IPropertyHandle& KeyedPropertyHandle) override;
	virtual EPropertyKeyedStatus GetPropertyKeyedStatus(TSharedPtr<ISequencer>& Sequencer, const IPropertyHandle& PropertyHandle) const override;

#if WITH_EDITOR
	virtual void PostEditUndo() override;
#endif

};

UCLASS(EditInlineNew, CollapseCategories)
class UAnimDetailControlsProxyFloat : public UAnimDetailControlsKeyedProxy
{
	GENERATED_BODY()
public:

	//UControlRigControlsProxy
	virtual bool PropertyIsOnProxy(FProperty* Property, FProperty* MemberProperty) override;
	virtual void ValueChanged() override;
	virtual EControlRigContextChannelToKey GetChannelToKeyFromPropertyName(const FName& InPropertyName) const override;
	virtual EControlRigContextChannelToKey GetChannelToKeyFromChannelName(const FString& InChannelName) const override;
	virtual bool IsMultiple(const FName& InPropertyName) const override;
	virtual void SetControlRigElementValueFromCurrent(UControlRig* ControlRig, FRigControlElement* ControlElement, const FRigControlModifiedContext& Context) override;
	virtual void GetChannelSelectionState(TWeakPtr<FCurveEditor>& CurveEditor, FAnimDetailVectorSelection& OutLocationSelection, FAnimDetailVectorSelection& OutRotationSelection,
		FAnimDetailVectorSelection& OutScaleSelection) override;
	virtual void UpdatePropertyNames(IDetailLayoutBuilder& DetailBuilder);
	virtual void SetBindingValueFromCurrent(UObject* InObject, TSharedPtr<FTrackInstancePropertyBindings>& Binding, FRigControlModifiedContext& Context, bool bInteractive = false) override;

	FAnimDetailPropertyState State;

	UPROPERTY(EditAnywhere, Interp, Category = "Float", meta = (Delta = "0.05", SliderExponent = "1", LinearDeltaSensitivity = "1"))
	FAnimDetailProxyFloat Float;
	
private:
	void ClearMultipleFlags();
	void SetMultipleFlags(const float& ValA, const float& ValB);
};

UCLASS(EditInlineNew, CollapseCategories)
class UAnimDetailControlsProxyBool : public UAnimDetailControlsKeyedProxy
{
	GENERATED_BODY()
public:

	//UControlRigControlsProxy
	virtual bool PropertyIsOnProxy(FProperty* Property, FProperty* MemberProperty) override;
	virtual void ValueChanged() override;
	virtual EControlRigContextChannelToKey GetChannelToKeyFromPropertyName(const FName& InPropertyName) const override;
	virtual EControlRigContextChannelToKey GetChannelToKeyFromChannelName(const FString& InChannelName) const override;
	virtual bool IsMultiple(const FName& InPropertyName) const override;
	virtual void SetControlRigElementValueFromCurrent(UControlRig* ControlRig, FRigControlElement* ControlElement, const FRigControlModifiedContext& Context) override;
	virtual void GetChannelSelectionState(TWeakPtr<FCurveEditor>& CurveEditor, FAnimDetailVectorSelection& OutLocationSelection, FAnimDetailVectorSelection& OutRotationSelection,
		FAnimDetailVectorSelection& OutScaleSelection) override;
	virtual void UpdatePropertyNames(IDetailLayoutBuilder& DetailBuilder);
	virtual void SetBindingValueFromCurrent(UObject* InObject, TSharedPtr<FTrackInstancePropertyBindings>& Binding, FRigControlModifiedContext& Context, bool bInteractive = false) override;

	
	FAnimDetailPropertyState State;

	UPROPERTY(EditAnywhere, Interp, Category = "Bool")
	FAnimDetailProxyBool Bool;

private:
	void ClearMultipleFlags();
	void SetMultipleFlags(const bool& ValA, const bool& ValB);
};

UCLASS(EditInlineNew, CollapseCategories)
class UAnimDetailControlsProxyInteger : public UAnimDetailControlsKeyedProxy
{
	GENERATED_BODY()
public:

	//UControlRigControlsProxy
	virtual bool PropertyIsOnProxy(FProperty* Property, FProperty* MemberProperty) override;
	virtual void ValueChanged() override;
	virtual EControlRigContextChannelToKey GetChannelToKeyFromPropertyName(const FName& InPropertyName) const override;
	virtual EControlRigContextChannelToKey GetChannelToKeyFromChannelName(const FString& InChannelName) const override;
	virtual bool IsMultiple(const FName& InPropertyName) const override;
	virtual void SetControlRigElementValueFromCurrent(UControlRig* ControlRig, FRigControlElement* ControlElement, const FRigControlModifiedContext& Context) override;
	virtual void GetChannelSelectionState(TWeakPtr<FCurveEditor>& CurveEditor, FAnimDetailVectorSelection& OutLocationSelection, FAnimDetailVectorSelection& OutRotationSelection,
		FAnimDetailVectorSelection& OutScaleSelection) override;
	virtual void UpdatePropertyNames(IDetailLayoutBuilder& DetailBuilder);
	virtual void SetBindingValueFromCurrent(UObject* InObject, TSharedPtr<FTrackInstancePropertyBindings>& Binding, FRigControlModifiedContext& Context, bool bInteractive = false) override;


	FAnimDetailPropertyState State;

	UPROPERTY(EditAnywhere, Interp, Category = "Integer")
	FAnimDetailProxyInteger Integer;

private:
	void ClearMultipleFlags();
	void SetMultipleFlags(const int64& ValA, const int64& ValB);
};

UCLASS(EditInlineNew, CollapseCategories)
class UAnimDetailControlsProxyEnum : public UAnimDetailControlsKeyedProxy
{
	GENERATED_BODY()
public:

	//UControlRigControlsProxy
	virtual bool PropertyIsOnProxy(FProperty* Property, FProperty* MemberProperty) override;
	virtual void ValueChanged() override;
	virtual EControlRigContextChannelToKey GetChannelToKeyFromPropertyName(const FName& InPropertyName) const override;
	virtual EControlRigContextChannelToKey GetChannelToKeyFromChannelName(const FString& InChannelName) const override;
	virtual bool IsMultiple(const FName& InPropertyName) const override;
	virtual void SetControlRigElementValueFromCurrent(UControlRig* ControlRig, FRigControlElement* ControlElement, const FRigControlModifiedContext& Context) override;
	virtual void GetChannelSelectionState(TWeakPtr<FCurveEditor>& CurveEditor, FAnimDetailVectorSelection& OutLocationSelection, FAnimDetailVectorSelection& OutRotationSelection,
		FAnimDetailVectorSelection& OutScaleSelection) override;
	virtual void UpdatePropertyNames(IDetailLayoutBuilder& DetailBuilder);


	FAnimDetailPropertyState State;

	UPROPERTY(EditAnywhere, Interp, Category = "Enum")
	FControlRigEnumControlProxyValue Enum;

private:
	void ClearMultipleFlags();
	void SetMultipleFlags(const int32& ValA, const int32& ValB);
};


UCLASS(EditInlineNew,CollapseCategories)
class UAnimDetailControlsProxyTransform : public UAnimDetailControlsKeyedProxy
{
	GENERATED_BODY()
public:

	//UControlRigControlsProxy
	virtual bool PropertyIsOnProxy(FProperty* Property, FProperty* MemberProperty) override;
	virtual void ValueChanged() override;
	virtual EControlRigContextChannelToKey GetChannelToKeyFromPropertyName(const FName& InPropertyName) const override;
	virtual EControlRigContextChannelToKey GetChannelToKeyFromChannelName(const FString& InChannelName) const override;
	virtual bool IsMultiple(const FName& InPropertyName) const override;
	virtual void SetControlRigElementValueFromCurrent(UControlRig* ControlRig, FRigControlElement* ControlElement, const FRigControlModifiedContext& Context) override;
	virtual void SetBindingValueFromCurrent(UObject* InObject, TSharedPtr<FTrackInstancePropertyBindings>& Binding, FRigControlModifiedContext& Context, bool bInteractive = false) override;
	virtual void GetChannelSelectionState(TWeakPtr<FCurveEditor>& CurveEditor, FAnimDetailVectorSelection& OutLocationSelection, FAnimDetailVectorSelection& OutRotationSelection,
		FAnimDetailVectorSelection& OutScaleSelection) override;

	UPROPERTY(EditAnywhere, Interp, Category = Transform)
	FAnimDetailProxyLocation Location;

	UPROPERTY(EditAnywhere, Interp, Category = Transform)
	FAnimDetailProxyRotation Rotation;

	UPROPERTY(EditAnywhere, Interp, Category = Transform)
	FAnimDetailProxyScale Scale;

private:
	void ClearMultipleFlags();
	void SetMultipleFlags(const FEulerTransform& ValA, const FEulerTransform& ValB);
};


UCLASS(EditInlineNew, CollapseCategories)
class UAnimDetailControlsProxyLocation : public UAnimDetailControlsKeyedProxy
{
	GENERATED_BODY()
public:

	//UControlRigControlsProxy
	virtual bool PropertyIsOnProxy(FProperty* Property, FProperty* MemberProperty) override;
	virtual void ValueChanged() override;
	virtual EControlRigContextChannelToKey GetChannelToKeyFromPropertyName(const FName& InPropertyName) const override;
	virtual EControlRigContextChannelToKey GetChannelToKeyFromChannelName(const FString& InChannelName) const override;
	virtual void SetControlRigElementValueFromCurrent(UControlRig* ControlRig, FRigControlElement* ControlElement, const FRigControlModifiedContext& Context) override;
	virtual bool IsMultiple(const FName& InPropertyName) const override;
	virtual void GetChannelSelectionState(TWeakPtr<FCurveEditor>& CurveEditor, FAnimDetailVectorSelection& OutLocationSelection, FAnimDetailVectorSelection& OutRotationSelection,
		FAnimDetailVectorSelection& OutScaleSelection) override;

	UPROPERTY(EditAnywhere, Interp, Category = Location)
	FAnimDetailProxyLocation Location;

	private:
	void ClearMultipleFlags();
	void SetMultipleFlags(const FVector3f& ValA, const FVector3f& ValB);
};

UCLASS(EditInlineNew, CollapseCategories)
class UAnimDetailControlsProxyRotation : public UAnimDetailControlsKeyedProxy
{
	GENERATED_BODY()
public:

	//UControlRigControlsProxy
	virtual bool PropertyIsOnProxy(FProperty* Property, FProperty* MemberProperty) override;
	virtual void ValueChanged() override;
	virtual EControlRigContextChannelToKey GetChannelToKeyFromPropertyName(const FName& InPropertyName) const override;
	virtual EControlRigContextChannelToKey GetChannelToKeyFromChannelName(const FString& InChannelName) const override;
	virtual void SetControlRigElementValueFromCurrent(UControlRig* ControlRig, FRigControlElement* ControlElement, const FRigControlModifiedContext& Context) override;
	virtual bool IsMultiple(const FName& InPropertyName) const override;
	virtual void GetChannelSelectionState(TWeakPtr<FCurveEditor>& CurveEditor, FAnimDetailVectorSelection& OutLocationSelection, FAnimDetailVectorSelection& OutRotationSelection,
		FAnimDetailVectorSelection& OutScaleSelection) override;

	UPROPERTY(EditAnywhere, Interp, Category = Rotation)
	FAnimDetailProxyRotation Rotation;

private:
	void ClearMultipleFlags();
	void SetMultipleFlags(const FVector3f& ValA, const FVector3f& ValB);
};

UCLASS(EditInlineNew, CollapseCategories)
class UAnimDetailControlsProxyScale : public UAnimDetailControlsKeyedProxy
{
	GENERATED_BODY()
public:

	//UControlRigControlsProxy
	virtual bool PropertyIsOnProxy(FProperty* Property, FProperty* MemberProperty) override;
	virtual void ValueChanged() override;
	virtual EControlRigContextChannelToKey GetChannelToKeyFromPropertyName(const FName& InPropertyName) const override;
	virtual EControlRigContextChannelToKey GetChannelToKeyFromChannelName(const FString& InChannelName) const override;
	virtual void SetControlRigElementValueFromCurrent(UControlRig* ControlRig, FRigControlElement* ControlElement, const FRigControlModifiedContext& Context) override;
	virtual bool IsMultiple(const FName& InPropertyName) const override;
	virtual void GetChannelSelectionState(TWeakPtr<FCurveEditor>& CurveEditor, FAnimDetailVectorSelection& OutLocationSelection, FAnimDetailVectorSelection& OutRotationSelection,
		FAnimDetailVectorSelection& OutScaleSelection) override;

	UPROPERTY(EditAnywhere, Interp, Category = Scale)
	FAnimDetailProxyScale Scale;

private:
	void ClearMultipleFlags();
	void SetMultipleFlags(const FVector3f& ValA, const FVector3f& ValB);
};

UCLASS(EditInlineNew, CollapseCategories)
class UAnimDetailControlsProxyVector2D : public UAnimDetailControlsKeyedProxy
{
	GENERATED_BODY()
public:

	//UControlRigControlsProxy
	virtual bool PropertyIsOnProxy(FProperty* Property, FProperty* MemberProperty) override;
	virtual void ValueChanged() override;
	virtual EControlRigContextChannelToKey GetChannelToKeyFromPropertyName(const FName& InPropertyName) const override;
	virtual EControlRigContextChannelToKey GetChannelToKeyFromChannelName(const FString& InChannelName) const override;
	virtual void SetControlRigElementValueFromCurrent(UControlRig* ControlRig, FRigControlElement* ControlElement, const FRigControlModifiedContext& Context) override;
	virtual bool IsMultiple(const FName& InPropertyName) const override;
	virtual void GetChannelSelectionState(TWeakPtr<FCurveEditor>& CurveEditor, FAnimDetailVectorSelection& OutLocationSelection, FAnimDetailVectorSelection& OutRotationSelection,
		FAnimDetailVectorSelection& OutScaleSelection) override;

	UPROPERTY(EditAnywhere, Interp, Category = Vector2D)
	FAnimDetailProxyVector2D Vector2D;

private:
	void ClearMultipleFlags();
	void SetMultipleFlags(const FVector2D& ValA, const FVector2D& ValB);
};


