// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


template<typename T>
class TMediaNoncopyable
	{
protected:
	TMediaNoncopyable() = default;
	~TMediaNoncopyable() = default;
private:
	TMediaNoncopyable(const TMediaNoncopyable&) = delete;
	TMediaNoncopyable& operator = (const TMediaNoncopyable&) = delete;
	};


