#pragma once
#include "Quaternion.h"

namespace Vulture
{
	class PerspectiveCamera
	{
	public:

		PerspectiveCamera() = default;
		~PerspectiveCamera() = default;

		PerspectiveCamera(const PerspectiveCamera& other);
		PerspectiveCamera(PerspectiveCamera&& other) noexcept;
		PerspectiveCamera& operator=(const PerspectiveCamera& other);
		PerspectiveCamera& operator=(PerspectiveCamera&& other) noexcept;

		Quaternion Rotation{};
		glm::vec3 Translation{ 0.0f };
		glm::mat4 ProjMat{ 1.0f };
		glm::mat4 ViewMat{ 1.0f };
		glm::vec2 NearFar{ 0.0f, 0.0f };

		float FOV = 45.0f;
		float AspectRatio = 1.0f;

		void Reset();

		void SetPerspectiveMatrix(float fov, float aspectRatio, float _near, float _far);

		void UpdateViewMatrix();
		void UpdateProjMatrix();

		void AddRotation(const glm::vec3& vec);
		void AddPitch(float pitch);
		void AddYaw(float yaw);
		void AddRoll(float roll);

		glm::mat4 GetProjView();
		inline const glm::vec3 GetFrontVec() const { return Rotation.GetFrontVec(); }
		inline const glm::vec3 GetRightVec() const { return Rotation.GetRightVec(); }
		inline const glm::vec3 GetUpVec() const { return Rotation.GetUpVec(); }
		inline const float GetAspectRatio() const { return AspectRatio; }
	};

}