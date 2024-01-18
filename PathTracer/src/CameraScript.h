#pragma once
#include <Vulture.h>

#include "glm/gtc/matrix_transform.hpp"

class CameraScript : public Vulture::ScriptInterface
{
public:
	CameraScript() {}
	~CameraScript() {}

	void OnCreate() override
	{
		auto cameraCp = &m_Entity.GetComponent<Vulture::CameraComponent>();
		cameraCp->SetZoom(1.0f);
		cameraCp->SetPerspectiveMatrix(45.0f, m_Entity.GetScene()->GetWindow()->GetAspectRatio(), 0.1f, 100.0f);

		cameraCp->Translation.z = -10.0f;
	}

	void OnDestroy() override
	{

	}

	void OnUpdate(double deltaTime) override
	{
		if (m_CameraLocked)
		{
			return;
		}

		m_Timer += deltaTime;
		auto& cameraComponent = GetComponent<Vulture::CameraComponent>();

		// Translation
		if (Vulture::Input::IsKeyPressed(VL_KEY_A))
		{
			cameraComponent.Translation += (float)deltaTime * m_MovementSpeed * cameraComponent.GetRightVec();
		}
		if (Vulture::Input::IsKeyPressed(VL_KEY_D))
		{
			cameraComponent.Translation -= (float)deltaTime * m_MovementSpeed * cameraComponent.GetRightVec();
		}
		if (Vulture::Input::IsKeyPressed(VL_KEY_W))
		{
			cameraComponent.Translation += (float)deltaTime * m_MovementSpeed * cameraComponent.GetFrontVec();
		}
		if (Vulture::Input::IsKeyPressed(VL_KEY_S))
		{
			cameraComponent.Translation -= (float)deltaTime * m_MovementSpeed * cameraComponent.GetFrontVec();
		}
		if (Vulture::Input::IsKeyPressed(VL_KEY_SPACE))
		{
			cameraComponent.Translation += (float)deltaTime * m_MovementSpeed * cameraComponent.GetUpVec();
		}
		if (Vulture::Input::IsKeyPressed(VL_KEY_LEFT_SHIFT))
		{
			cameraComponent.Translation -= (float)deltaTime * m_MovementSpeed * cameraComponent.GetUpVec();
		}

		// Rotation
		glm::vec2 mousePosition = Vulture::Input::GetMousePosition();
		if (Vulture::Input::IsMousePressed(0))
		{
			// Pitch
			if (m_LastMousePosition.y != mousePosition.y)
			{
				cameraComponent.AddPitch((m_LastMousePosition.y - mousePosition.y) * m_RotationSpeed);
			}

			// Yaw
			if (m_LastMousePosition.x != mousePosition.x)
			{
				cameraComponent.AddYaw(-(m_LastMousePosition.x - mousePosition.x) * m_RotationSpeed);
			}

			// Roll
			if (Vulture::Input::IsKeyPressed(VL_KEY_Q))
			{
				cameraComponent.AddRoll(200.0f * (float)deltaTime * m_RotationSpeed);
			}
			if (Vulture::Input::IsKeyPressed(VL_KEY_E))
			{
				cameraComponent.AddRoll(-200.0f * (float)deltaTime * m_RotationSpeed);
			}
		}
		m_LastMousePosition = mousePosition;

		cameraComponent.UpdateViewMatrix();
	}

	float m_MovementSpeed = 5.0f;
	float m_RotationSpeed = 0.1f;
	bool m_CameraLocked = false;
private:
	double m_Timer = 0.0;
	glm::vec2 m_LastMousePosition{0.0f};
};