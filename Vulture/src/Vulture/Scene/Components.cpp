#include "pch.h"
#include "Components.h"
#include "Renderer/Renderer.h"
#include "Renderer/AssetManager.h"

namespace Vulture
{
	SkyboxComponent::SkyboxComponent(const std::string filepath)
	{
		Ref<Image> tempImage = std::make_shared<Image>(filepath);

		Image::CreateInfo imageInfo = {};
		imageInfo.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		imageInfo.Format = VK_FORMAT_R32G32B32A32_SFLOAT;
		imageInfo.Height = 1024;
		imageInfo.Width = 1024;
		imageInfo.LayerCount = 6;
		imageInfo.Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		imageInfo.SamplerInfo = {};
		imageInfo.Tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.Type = Image::ImageType::Cubemap;
		imageInfo.Usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		SkyboxImage = AssetManager::CreateTexture(imageInfo);

		Renderer::EnvMapToCubemapPass(tempImage, SkyboxImage);
	}

	void ScriptComponent::InitializeScripts()
	{
		for (auto& script : Scripts)
		{
			script->OnCreate();
		}
	}

	void ScriptComponent::UpdateScripts(double deltaTime)
	{
		for (auto& script : Scripts)
		{
			script->OnUpdate(deltaTime);
		}
	}

	void ScriptComponent::DestroyScripts()
	{
		for (auto& script : Scripts)
		{
			script->OnDestroy();
		}
	}

	ScriptComponent::~ScriptComponent()
	{
		for (auto& script : Scripts)
		{
			delete script;
		}
	}

	SpriteComponent::SpriteComponent(const glm::vec2 atlasOffsets)
		: AtlasOffsets(atlasOffsets)
	{

	}

	void SpriteComponent::Draw()
	{
		// ????
	}

	TransformComponent::TransformComponent(const Transform& transform)
		: transform(transform)
	{

	}

	TransformComponent::TransformComponent(const glm::vec3& translation, const glm::vec3& rotation, const glm::vec3& scale)
		: transform({ translation, rotation, scale })
	{

	}

	StaticTransformComponent::StaticTransformComponent(const Transform& transform)
		: transform(transform)
	{

	}

	StaticTransformComponent::StaticTransformComponent(const glm::vec3& translation, const glm::vec3& rotation, const glm::vec3& scale)
		: transform({ translation, rotation, scale })
	{

	}

	void CameraComponent::Reset()
	{
		this->Translation = glm::vec3(0.0f);
		Zoom = 1.0f;

		Rotation.Reset();

		UpdateViewMatrix();
	}

	void CameraComponent::SetOrthographicMatrix(glm::vec4 leftRightBottomTop, float _near, float _far, float aspectRatio)
	{
		ProjMat = glm::ortho(leftRightBottomTop.x * aspectRatio * Zoom, leftRightBottomTop.y * aspectRatio * Zoom, leftRightBottomTop.z * Zoom, leftRightBottomTop.w * Zoom, _near, _far);
	}

	void CameraComponent::SetPerspectiveMatrix(float fov, float aspectRatio, float _near, float _far)
	{
		AspectRatio = aspectRatio;
		ProjMat = glm::perspective(glm::radians(fov), aspectRatio, _near, _far);
	}

	void CameraComponent::SetZoom(float zoom)
	{
		Zoom = zoom;
	}

	void CameraComponent::UpdateViewMatrix()
	{
		ViewMat = Rotation.GetMat4();
		ViewMat = glm::translate(ViewMat, Translation);

		// Invert Y axis so that image is not flipped
		ViewMat = glm::scale(ViewMat, glm::vec3(1.0f, -1.0f, 1.0f));
	}

	void CameraComponent::UpdateViewMatrixCustom(const glm::mat4 mat)
	{
		ViewMat = mat;
	}

	glm::mat4 CameraComponent::GetViewProj()
	{
		return ProjMat * ViewMat;
	}

	void CameraComponent::AddRotation(const glm::vec3& vec)
	{
		Rotation.AddAngles(vec);
	}

	void CameraComponent::AddPitch(float pitch)
	{
		Rotation.AddPitch(pitch);
	}

	void CameraComponent::AddYaw(float yaw)
	{
		Rotation.AddYaw(yaw);
	}

	void CameraComponent::AddRoll(float roll)
	{
		Rotation.AddRoll(roll);
	}

	ColliderComponent::ColliderComponent(const Vulture::Entity& entity, const std::string name)
		: Entity(entity), ColliderName(name)
	{

	}

	bool ColliderComponent::CheckCollision(ColliderComponent& otherCollider)
	{
		TransformComponent transform = Entity.GetComponent<TransformComponent>().transform;
		TransformComponent otherTransform = otherCollider.Entity.GetComponent<TransformComponent>().transform;

		glm::vec2 position = transform.transform.GetTranslation() - transform.transform.GetScale();
		glm::vec2 size = transform.transform.GetScale() * 2.0f;

		glm::vec2 otherPosition = otherTransform.transform.GetTranslation() - otherTransform.transform.GetScale();
		glm::vec2 otherSize = otherTransform.transform.GetScale() * 2.0f;

		if (position.x < otherPosition.x + otherSize.x &&
			position.x + size.x > otherPosition.x &&
			position.y < otherPosition.y + otherSize.y &&
			position.y + size.x > otherPosition.y)
		{
			return true;
		}
		return false;
	}

	TextComponent::TextComponent(const Text::CreateInfo& createInfo)
		: Text(createInfo)
	{

	}

	void TextComponent::ChangeText(const std::string& string, float kerningOffset /*= 0.0f*/)
	{
		Text.ChangeText(string, kerningOffset);
	}

	ModelComponent::ModelComponent(const std::string filepath)
	{
		Model = Vulture::AssetManager::LoadModel(filepath);
	}

}