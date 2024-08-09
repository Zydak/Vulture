// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "pch.h"
#include "OrthographicCamera.h"

namespace Vulture
{

	OrthographicCamera::OrthographicCamera(const OrthographicCamera& other)
	{
		Rotation	= other.Rotation;
		Translation = other.Translation;
		ProjMat		= other.ProjMat;
		ViewMat		= other.ViewMat;
		NearFar		= other.NearFar;
		Zoom		= other.Zoom;
	}

	OrthographicCamera::OrthographicCamera(OrthographicCamera&& other) noexcept
	{
		Rotation	= std::move(other.Rotation);
		Translation = std::move(other.Translation);
		ProjMat		= std::move(other.ProjMat);
		ViewMat		= std::move(other.ViewMat);
		NearFar		= std::move(other.NearFar);
		Zoom		= std::move(other.Zoom);
	}

	OrthographicCamera& OrthographicCamera::operator=(const OrthographicCamera& other)
	{
		Rotation	= other.Rotation;
		Translation = other.Translation;
		ProjMat		= other.ProjMat;
		ViewMat		= other.ViewMat;
		NearFar		= other.NearFar;
		Zoom		= other.Zoom;

		return *this;
	}

	OrthographicCamera& OrthographicCamera::operator=(OrthographicCamera&& other) noexcept
	{
		Rotation	= std::move(other.Rotation);
		Translation = std::move(other.Translation);
		ProjMat		= std::move(other.ProjMat);
		ViewMat		= std::move(other.ViewMat);
		NearFar		= std::move(other.NearFar);
		Zoom		= std::move(other.Zoom);

		return *this;
	}

	void OrthographicCamera::Reset()
	{
		this->Translation = glm::vec3(0.0f);
		Zoom = 1.0f;

		Rotation.Reset();

		UpdateViewMatrix();
	}

	void OrthographicCamera::SetOrthographicMatrix(const glm::vec4& leftRightBottomTop, float _near, float _far)
	{
		LeftRightBottomTopOrtho = leftRightBottomTop;
		NearFar = { _near, _far };
		ProjMat = glm::ortho(LeftRightBottomTopOrtho.x * Zoom, LeftRightBottomTopOrtho.y * Zoom, LeftRightBottomTopOrtho.z * Zoom, LeftRightBottomTopOrtho.w * Zoom, _near, _far);
	}

	void OrthographicCamera::SetZoom(float zoom)
	{
		// Revert previous zoom
		Zoom = zoom;

		ProjMat = glm::ortho(LeftRightBottomTopOrtho.x * Zoom, LeftRightBottomTopOrtho.y * Zoom, LeftRightBottomTopOrtho.z * Zoom, LeftRightBottomTopOrtho.w * Zoom, NearFar.x, NearFar.y);
	}

	void OrthographicCamera::UpdateViewMatrix()
	{
		ViewMat = Rotation.GetMat4();
		ViewMat = glm::translate(ViewMat, Translation);

		// Invert Y axis so that image is not flipped
		ViewMat = glm::scale(ViewMat, glm::vec3(1.0f, -1.0f, 1.0f));
	}

	void OrthographicCamera::UpdateViewMatrixCustom(const glm::mat4 mat)
	{
		ViewMat = mat;
	}

	glm::mat4 OrthographicCamera::GetProjView()
	{
		return ProjMat * ViewMat;
	}

	void OrthographicCamera::AddRotation(const glm::vec3& vec)
	{
		Rotation.AddAngles(vec);
	}

	void OrthographicCamera::AddPitch(float pitch)
	{
		Rotation.AddPitch(pitch);
	}

	void OrthographicCamera::AddYaw(float yaw)
	{
		Rotation.AddYaw(yaw);
	}

	void OrthographicCamera::AddRoll(float roll)
	{
		Rotation.AddRoll(roll);
	}

}