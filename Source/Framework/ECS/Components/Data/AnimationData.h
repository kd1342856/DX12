#pragma once
struct RuntimeAnimationData
{
	int AnimationIndex = -1;
	float ProgressTime = 0.0f;
	float Speed = 1.0f;
	bool IsPlaying = false;
	bool IsLoop = true;

	// 0以上の場合、通常のduration終端ではなくこの時刻で非ループ再生を止める。
	// (例: 歩行モーションを「足がそろう位置」で止めたい時に使う。-1は未指定)
	float StopAtTime = -1.0f;
};

struct AnimationDataComponent
{
	RuntimeAnimationData currentAnim;	// 通常用（1つのアニメーション）
	// キー=アニメーションIndex、値=再生状態。ドアなど複数アニメーションの同時管理に使用
	std::map<int, RuntimeAnimationData> multiAnims;
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