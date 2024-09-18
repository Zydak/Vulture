#include "pch.h"
#include "PerspectiveCamera.h"

namespace Vulture
{

	PerspectiveCamera::PerspectiveCamera(const PerspectiveCamera& other)
	{
		Rotation	= other.Rotation;
		Translation = other.Translation;
		ProjMat		= other.ProjMat;
		ViewMat		= other.ViewMat;
		NearFar		= other.NearFar;
		FOV			= other.FOV;
		AspectRatio = other.AspectRatio;
	}

	PerspectiveCamera::PerspectiveCamera(PerspectiveCamera&& other) noexcept
	{
		Rotation	= std::move(other.Rotation);
		Translation	= std::move(other.Translation);
		ProjMat		= std::move(other.ProjMat);
		ViewMat		= std::move(other.ViewMat);
		NearFar		= std::move(other.NearFar);
		FOV			= std::move(other.FOV);
		AspectRatio = std::move(other.AspectRatio);
	}

	PerspectiveCamera& PerspectiveCamera::operator=(const PerspectiveCamera& other) 
	{
		Rotation	= other.Rotation;
		Translation = other.Translation;
		ProjMat		= other.ProjMat;
		ViewMat		= other.ViewMat;
		NearFar		= other.NearFar;
		FOV			= other.FOV;
		AspectRatio = other.AspectRatio;

		return *this;
	}

	PerspectiveCamera& PerspectiveCamera::operator=(PerspectiveCamera&& other) noexcept
	{
		Rotation	= std::move(other.Rotation);
		Translation = std::move(other.Translation);
		ProjMat		= std::move(other.ProjMat);
		ViewMat		= std::move(other.ViewMat);
		NearFar		= std::move(other.NearFar);
		FOV			= std::move(other.FOV);
		AspectRatio = std::move(other.AspectRatio);

		return *this;
	}

	void PerspectiveCamera::Reset()
	{
		this->Translation = glm::vec3(0.0f);

		Rotation.Reset();

		UpdateViewMatrix();
	}

	void PerspectiveCamera::SetPerspectiveMatrix(float fov, float aspectRatio, float _near, float _far)
	{
		FOV = fov;
		AspectRatio = aspectRatio;
		NearFar.x = _near;
		NearFar.y = _far;
		ProjMat = glm::perspective(glm::radians(fov), aspectRatio, _near, _far);
	}

	void PerspectiveCamera::UpdateViewMatrix()
	{
		ViewMat = Rotation.GetMat4();
		ViewMat = glm::translate(ViewMat, Translation);
	}

	void PerspectiveCamera::UpdateProjMatrix()
	{
		ProjMat = glm::perspective(glm::radians(FOV), AspectRatio, NearFar.x, NearFar.y);
	}

	glm::mat4 PerspectiveCamera::GetProjView()
	{
		return ProjMat * ViewMat;
	}

	void PerspectiveCamera::AddRotation(const glm::vec3& vec)
	{
		Rotation.AddAngles(vec);
	}

	void PerspectiveCamera::AddPitch(float pitch)
	{
		Rotation.AddPitch(pitch);
	}

	void PerspectiveCamera::AddYaw(float yaw)
	{
		Rotation.AddYaw(yaw);
	}

	void PerspectiveCamera::AddRoll(float roll)
	{
		Rotation.AddRoll(roll);
	}

}