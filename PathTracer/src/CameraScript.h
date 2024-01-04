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
		timer += deltaTime;
		float movementSpeed = 0.2f;
		auto& cameraComponent = GetComponent<Vulture::CameraComponent>();

		if (orbitCamera)
		{
			cameraComponent.Translation.x = (float)sin(timer) * 5.0f;
			cameraComponent.Translation.z = (float)cos(timer) * 5.0f;

			glm::vec3 viewDirection = glm::normalize(-cameraComponent.Translation);

			glm::mat4 customMat = glm::lookAt(cameraComponent.Translation, (cameraComponent.Translation + viewDirection) + glm::vec3(0.0f, 0.1f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

			cameraComponent.UpdateViewMatrixCustom(customMat);
		}
		else
		{
			if (Vulture::Input::IsKeyPressed(VL_KEY_A))
			{
				cameraComponent.Translation.x += 0.5f * movementSpeed;
			}
			if (Vulture::Input::IsKeyPressed(VL_KEY_D))
			{
				cameraComponent.Translation.x -= 0.5f * movementSpeed;
			}
			if (Vulture::Input::IsKeyPressed(VL_KEY_W))
			{
				cameraComponent.Translation.z += 0.5f * movementSpeed;
			}
			if (Vulture::Input::IsKeyPressed(VL_KEY_S))
			{
				cameraComponent.Translation.z -= 0.5f * movementSpeed;
			}
			if (Vulture::Input::IsKeyPressed(VL_KEY_SPACE))
			{
				cameraComponent.Translation.y -= 0.5f * movementSpeed;
			}
			if (Vulture::Input::IsKeyPressed(VL_KEY_LEFT_SHIFT))
			{
				cameraComponent.Translation.y += 0.5f * movementSpeed;
			}

			cameraComponent.UpdateViewMatrix();
		}

	}

	bool orbitCamera = true;
private:
	double timer = 0.0;
};