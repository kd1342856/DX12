#pragma once
struct RuntimeAnimationData
{
	int AnimationIndex = -1;
	float ProgressTime = 0.0f;
	float Speed = 1.0f;
	bool IsPlaying = false;
	bool IsLoop = true;
};

struct AnimationDataComponent
{
	RuntimeAnimationData currentAnim;
};

struct AnimationKeyVector
{
	float			time;
	Math::Vector3	value;
};

struct AnimationKeyQuaternion
{
	float				time;
	Math::Quaternion	value;
};

struct AnimationChannel
{
	std::string							nodeName;
	std::vector<AnimationKeyVector>		positionKeys;
	std::vector<AnimationKeyQuaternion> rotationKeys;
	std::vector<AnimationKeyVector>		scalingKeys;
};

struct AnimationData
{
	std::string						name;
	float							duration = 0.0f;
	float							ticksPerSecond = 0.0f;
	std::vector<AnimationChannel>	channels;
};