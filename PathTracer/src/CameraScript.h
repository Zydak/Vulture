#pragma once
#include <Vulture.h>

class CameraScript : public Vulture::ScriptInterface
{
public:
	CameraScript() {}
	~CameraScript() {}

	void OnCreate() override
	{
		auto cameraCp = &m_Entity.GetComponent<Vulture::CameraComponent>();
		cameraCp->SetZoom(1.0f);
		cameraCp->SetOrthographicMatrix({ -20.0f, 20.0f, -20.0f, 20.0f }, 0.1f, 100.0f, m_Entity.GetScene()->GetWindow()->GetAspectRatio());
	}

	void OnDestroy() override
	{

	}

	void OnUpdate(double deltaTime) override
	{
		float movementSpeed = 0.2f;
		auto& cameraComponent = GetComponent<Vulture::CameraComponent>();

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
			cameraComponent.Translation.y -= 0.5f * movementSpeed;
		}
		if (Vulture::Input::IsKeyPressed(VL_KEY_S))
		{
			cameraComponent.Translation.y += 0.5f * movementSpeed;
		}

		cameraComponent.UpdateViewMatrix();

	}

private:

};