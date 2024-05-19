#pragma once
#include "Quaternion.h"

namespace Vulture
{
	class OrthographicCamera
	{
	public:
		OrthographicCamera() = default;
		~OrthographicCamera() = default;

		OrthographicCamera(const OrthographicCamera& other);
		OrthographicCamera(OrthographicCamera&& other) noexcept;
		OrthographicCamera& operator=(const OrthographicCamera& other);
		OrthographicCamera& operator=(OrthographicCamera&& other) noexcept;

		Quaternion Rotation{};
		glm::vec3 Translation{ 0.0f };
		glm::mat4 ProjMat{ 1.0f };
		glm::mat4 ViewMat{ 1.0f };
		glm::vec4 LeftRightBottomTopOrtho;
		glm::vec2 NearFar;

		float Zoom = 1.0f;

		void Reset();

		void SetOrthographicMatrix(glm::vec4 leftRightBottomTop, float _near, float _far);
		void SetZoom(float zoom);

		void UpdateViewMatrix();
		void UpdateViewMatrixCustom(const glm::mat4 mat);

		void AddRotation(const glm::vec3& vec);
		void AddPitch(float pitch);
		void AddYaw(float yaw);
		void AddRoll(float roll);

		glm::mat4 GetProjView();
		inline const glm::vec3 GetFrontVec() const { return Rotation.GetFrontVec(); }
		inline const glm::vec3 GetRightVec() const { return Rotation.GetRightVec(); }
		inline const glm::vec3 GetUpVec() const { return Rotation.GetUpVec(); }
	};

}