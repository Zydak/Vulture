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
		m_Timer += deltaTime;
		auto& cameraComponent = GetComponent<Vulture::CameraComponent>();

		if (m_OrbitCamera)
		{
			cameraComponent.Translation.x = (float)sin(m_Timer * m_MovementSpeed * deltaTime) * 5.0f;
			cameraComponent.Translation.y = 0.0f;
			cameraComponent.Translation.z = (float)cos(m_Timer * m_MovementSpeed * deltaTime) * 5.0f;

			glm::vec3 viewDirection = glm::normalize(-cameraComponent.Translation);

			glm::mat4 customMat = glm::lookAt(cameraComponent.Translation, (cameraComponent.Translation + viewDirection) + glm::vec3(0.0f, 0.1f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

			cameraComponent.UpdateViewMatrixCustom(customMat);
		}
		else
		{
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
				cameraComponent.Translation -= (float)deltaTime * m_MovementSpeed * cameraComponent.GetUpVec();
			}
			if (Vulture::Input::IsKeyPressed(VL_KEY_LEFT_SHIFT))
			{
				cameraComponent.Translation += (float)deltaTime * m_MovementSpeed * cameraComponent.GetUpVec();
			}

			// Rotation
			glm::vec2 mousePosition = Vulture::Input::GetMousePosition();
			if (Vulture::Input::IsMousePressed(0))
			{
				// Pitch
				if (m_LastMousePosition.y != mousePosition.y)
				{
					cameraComponent.AddPitch(-(m_LastMousePosition.y - mousePosition.y) * (float)deltaTime * m_RotationSpeed);
				}

				// Yaw
				if (m_LastMousePosition.x != mousePosition.x)
				{
					cameraComponent.AddYaw(-(m_LastMousePosition.x - mousePosition.x) * (float)deltaTime * m_RotationSpeed);
				}

				// Roll
				if (Vulture::Input::IsKeyPressed(VL_KEY_Q))
				{
					cameraComponent.AddRoll(-20.0f * (float)deltaTime * m_RotationSpeed);
				}
				if (Vulture::Input::IsKeyPressed(VL_KEY_E))
				{
					cameraComponent.AddRoll(20.0f * (float)deltaTime * m_RotationSpeed);
				}
			}
			m_LastMousePosition = mousePosition;

			cameraComponent.UpdateViewMatrix();
		}

	}

	bool m_OrbitCamera = false;
	float m_MovementSpeed = 20.0f;
	float m_RotationSpeed = 2.0f;
private:
	double m_Timer = 0.0;
	glm::vec2 m_LastMousePosition{0.0f};
};