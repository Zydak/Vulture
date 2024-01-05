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
			cameraComponent.Translation.x = (float)sin(m_Timer * m_MovementSpeed) * 5.0f;
			cameraComponent.Translation.y = 0.0f;
			cameraComponent.Translation.z = (float)cos(m_Timer * m_MovementSpeed) * 5.0f;

			glm::vec3 viewDirection = glm::normalize(-cameraComponent.Translation);

			glm::mat4 customMat = glm::lookAt(cameraComponent.Translation, (cameraComponent.Translation + viewDirection) + glm::vec3(0.0f, 0.1f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

			cameraComponent.UpdateViewMatrixCustom(customMat);
		}
		else
		{
			if (Vulture::Input::IsKeyPressed(VL_KEY_A))
			{
				cameraComponent.Translation += 0.2f * m_MovementSpeed * m_RightVector;
			}
			if (Vulture::Input::IsKeyPressed(VL_KEY_D))
			{
				cameraComponent.Translation -= 0.2f * m_MovementSpeed * m_RightVector;
			}
			if (Vulture::Input::IsKeyPressed(VL_KEY_W))
			{
				cameraComponent.Translation += 0.2f * m_MovementSpeed * m_FrontVector;
			}
			if (Vulture::Input::IsKeyPressed(VL_KEY_S))
			{
				cameraComponent.Translation -= 0.2f * m_MovementSpeed * m_FrontVector;
			}
			if (Vulture::Input::IsKeyPressed(VL_KEY_SPACE))
			{
				cameraComponent.Translation -= 0.2f * m_MovementSpeed * m_UpVector;
			}
			if (Vulture::Input::IsKeyPressed(VL_KEY_LEFT_SHIFT))
			{
				cameraComponent.Translation += 0.2f * m_MovementSpeed * m_UpVector;
			}

			glm::vec2 mousePosition = Vulture::Input::GetMousePosition();
			if (Vulture::Input::IsMousePressed(0))
			{
				if (m_LastMousePosition.x != mousePosition.x)
				{
					cameraComponent.Rotation.x -= (float)(m_LastMousePosition.x - mousePosition.x) * 0.1f;
				}
				if (m_LastMousePosition.y != mousePosition.y)
				{
					cameraComponent.Rotation.y -= (float)(m_LastMousePosition.y - mousePosition.y) * 0.1f;
				}
			}
			m_LastMousePosition = mousePosition;

			cameraComponent.UpdateViewMatrix();

			glm::mat4 view = cameraComponent.ViewMat;

			m_FrontVector = glm::vec3(view[0][2], view[1][2], view[2][2]);
			m_RightVector = glm::vec3(view[0][0], view[1][0], view[2][0]);
			m_UpVector = glm::vec3(view[0][1], view[1][1], view[2][1]);
		}

	}

	bool m_OrbitCamera = true;
	float m_MovementSpeed = 0.2f;
private:
	double m_Timer = 0.0;
	glm::vec2 m_LastMousePosition{0.0f};
	glm::vec3 m_FrontVector{ 0.0f, 0.0f, -1.0f };
	glm::vec3 m_UpVector{ 0.0f, 1.0f, 0.0f };
	glm::vec3 m_RightVector{ 1.0f, 0.0f, 0.0f };
};