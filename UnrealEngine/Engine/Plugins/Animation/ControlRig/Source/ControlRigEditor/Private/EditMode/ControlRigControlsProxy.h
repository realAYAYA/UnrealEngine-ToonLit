// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/LazyObjectPtr.h"
#include "TransformNoScale.h"
#include "EulerTransform.h"

#if WITH_EDITOR
#include "IPropertyTypeCustomization.h"
#endif

#include "ControlRigControlsProxy.generated.h"

struct FRigControlElement;
class UControlRig;
class IPropertyHandle;

UCLASS(Abstract)
class UControlRigControlsProxy : public UObject
{
	GENERATED_BODY()

public:
	UControlRigControlsProxy() : bSelected(false) {}
	virtual void SetName(const FName& InName) { Name = ControlName = InName; }
	virtual FName GetName() const { return Name; }
	virtual void ValueChanged() {}
	virtual void SelectionChanged(bool bInSelected);
	virtual void SetKey(const IPropertyHandle& KeyedPropertyHandle) {};

	// UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#if WITH_EDITOR
	virtual void PostEditUndo() override;
#endif
	void SetIsMultiple(bool bIsVal); 
	void SetIsIndividual(bool bIsVal);
public:

	FRigControlElement* GetControlElement() const;
	TWeakObjectPtr<UControlRig> ControlRig;

	UPROPERTY()
	bool bSelected;
	UPROPERTY(VisibleAnywhere, Category = "Control")
	FName ControlName;

	//if individual it will show up independently, this will happen for certain nested controls
	bool bIsIndividual = false;

protected:
	bool bIsMultiple = 0;
	FName Name;
};

UCLASS()
class UControlRigTransformControlProxy : public UControlRigControlsProxy
{
	GENERATED_BODY()
	UControlRigTransformControlProxy() {}

	// UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#if WITH_EDITOR
	virtual void PostEditUndo() override;
#endif

	//UControlRigControlsProxy
	virtual void ValueChanged() override;
	virtual void SetKey(const IPropertyHandle& KeyedPropertyHandle) override;

public:
	
	UPROPERTY(EditAnywhere, Interp, Category = "Control", meta = (SliderExponent = "1.0"))
	FEulerTransform Transform; //FTransform doesn't work with multiple values for some reason, so for now using eulertransform which does work
};


UCLASS()
class UControlRigEulerTransformControlProxy : public UControlRigControlsProxy
{
	GENERATED_BODY()
	UControlRigEulerTransformControlProxy() {}

	// UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#if WITH_EDITOR
	virtual void PostEditUndo() override;
#endif

	//UControlRigControlsProxy
	virtual void ValueChanged() override;
	virtual void SetKey(const IPropertyHandle& KeyedPropertyHandle) override;

public:

	UPROPERTY(EditAnywhere, Interp, Category = "Control", meta = (SliderExponent = "1.0"))
	FEulerTransform Transform;
};


UCLASS()
class UControlRigTransformNoScaleControlProxy : public UControlRigControlsProxy
{
	GENERATED_BODY()
	UControlRigTransformNoScaleControlProxy() {}

	// UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#if WITH_EDITOR
	virtual void PostEditUndo() override;
#endif

	//UControlRigControlsProxy
	virtual void ValueChanged() override;
	virtual void SetKey(const IPropertyHandle& KeyedPropertyHandle) override;

public:

	UPROPERTY(EditAnywhere, Interp, Category = "Control", meta = (SliderExponent = "1.0"))
	FTransformNoScale Transform;
};

UCLASS()
class UControlRigFloatControlProxy : public UControlRigControlsProxy
{
	GENERATED_BODY()
	UControlRigFloatControlProxy()  {}

	// UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#if WITH_EDITOR
	virtual void PostEditUndo() override;
#endif

	//UControlRigControlsProxy
	virtual void ValueChanged() override;
	virtual void SetKey(const IPropertyHandle& KeyedPropertyHandle) override;

public:

	UPROPERTY(EditAnywhere, Interp, Category = "Control", meta = (SliderExponent = "1.0"))
	float Float;
};

UCLASS()
class UControlRigIntegerControlProxy : public UControlRigControlsProxy
{
	GENERATED_BODY()
	UControlRigIntegerControlProxy() {}

	// UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#if WITH_EDITOR
	virtual void PostEditUndo() override;
#endif

	//UControlRigControlsProxy
	virtual void ValueChanged() override;
	virtual void SetKey(const IPropertyHandle& KeyedPropertyHandle) override;

public:

	UPROPERTY(EditAnywhere, Interp, Category = "Control")
	int32 Integer;
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

#if WITH_EDITOR

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

	UControlRigEnumControlProxy* ProxyBeingCustomized;
};
#endif

UCLASS()
class UControlRigEnumControlProxy : public UControlRigControlsProxy
{
	GENERATED_BODY()
	UControlRigEnumControlProxy() {}

	// UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#if WITH_EDITOR
	virtual void PostEditUndo() override;
#endif

	//UControlRigControlsProxy
	virtual void ValueChanged() override;
	virtual void SetKey(const IPropertyHandle& KeyedPropertyHandle) override;

public:

	UPROPERTY(EditAnywhere, Interp, Category = "Control")
	FControlRigEnumControlProxyValue Enum;
};

UCLASS()
class UControlRigVectorControlProxy : public UControlRigControlsProxy
{
	GENERATED_BODY()
	UControlRigVectorControlProxy() {}

	// UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#if WITH_EDITOR
	virtual void PostEditUndo() override;
#endif

	//UControlRigControlsProxy
	virtual void ValueChanged() override;
	virtual void SetKey(const IPropertyHandle& KeyedPropertyHandle) override;

public:

	UPROPERTY(EditAnywhere, Interp, Category = "Control", meta = (SliderExponent = "1.0"))
	FVector3f Vector;
};

UCLASS()
class UControlRigVector2DControlProxy : public UControlRigControlsProxy
{
	GENERATED_BODY()
	UControlRigVector2DControlProxy() {}

	// UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#if WITH_EDITOR
	virtual void PostEditUndo() override;
#endif

	//UControlRigControlsProxy
	virtual void ValueChanged() override;
	virtual void SetKey(const IPropertyHandle& KeyedPropertyHandle) override;

public:

	UPROPERTY(EditAnywhere, Interp, Category = "Control", meta = (SliderExponent = "1.0"))
	FVector2D Vector2D;
};

UCLASS()
class UControlRigBoolControlProxy : public UControlRigControlsProxy
{
	GENERATED_BODY()
	UControlRigBoolControlProxy() {}

	// UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#if WITH_EDITOR
	virtual void PostEditUndo() override;
#endif

	//UControlRigControlsProxy
	virtual void ValueChanged() override;
	virtual void SetKey(const IPropertyHandle& KeyedPropertyHandle) override;

public:

	UPROPERTY(EditAnywhere, Interp, Category = "Control")
	bool Bool;
};

USTRUCT()
struct FControlToProxyMap
{
	GENERATED_BODY();

	UPROPERTY()
	TMap <FName, TObjectPtr<UControlRigControlsProxy>> ControlToProxy;
};
/** Proxy in Details Panel */
UCLASS()
class UControlRigDetailPanelControlProxies :public UObject
{
	GENERATED_BODY()

	UControlRigDetailPanelControlProxies() {}
protected:

	UPROPERTY()
	TMap<TObjectPtr<UControlRig>, FControlToProxyMap> AllProxies; //proxies themselves contain weakobjectptr to the controlrig

	UPROPERTY()
	TArray< TObjectPtr<UControlRigControlsProxy>> SelectedProxies;


public:
	UControlRigControlsProxy* FindProxy(UControlRig* InControlRig, const FName& Name) const;
	void AddProxy(UControlRig* InControlRig, const FName& Name,  FRigControlElement* ControlElement);
	void RemoveProxy(UControlRig* InControlRig, const FName& Name );
	void ProxyChanged(UControlRig* InControlRig, const FName& Name);
	void RemoveAllProxies(UControlRig* InControlRig);
	void RecreateAllProxies(UControlRig* InControlRig);
	void SelectProxy(UControlRig* InControlRig, const FName& Name, bool bSelected);
	const TArray<UControlRigControlsProxy*>& GetSelectedProxies() const { return SelectedProxies;}
	bool IsSelected(UControlRig* InControlRig, const FName& Name) const;

};
