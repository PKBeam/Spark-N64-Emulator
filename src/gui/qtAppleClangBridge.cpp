#include <QtGui/QVulkanWindow>
#include <vulkan/vulkan.h>

extern "C" void apply_vulkan_features(QVulkanWindow* window) {

    constinit static auto features14 = VkPhysicalDeviceVulkan14Features{};
    features14.sType                 = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;
    features14.pushDescriptor        = VK_TRUE;

    constinit static auto features13          = VkPhysicalDeviceVulkan13Features{};
    features13.sType                          = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering               = VK_TRUE;
    features13.synchronization2               = VK_TRUE;
    features13.shaderDemoteToHelperInvocation = VK_TRUE;

    constinit static auto features12                         = VkPhysicalDeviceVulkan12Features{};
    features12.sType                                         = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.shaderInt8                                    = VK_TRUE;
    features12.uniformAndStorageBuffer8BitAccess             = VK_TRUE;
    features12.storagePushConstant8                          = VK_TRUE;
    features12.scalarBlockLayout                             = VK_TRUE;
    features12.descriptorBindingSampledImageUpdateAfterBind  = VK_TRUE;
    features12.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE;

    constinit static auto extendedDynamicState2Features = VkPhysicalDeviceExtendedDynamicState2FeaturesEXT{};
    extendedDynamicState2Features.sType                 = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT;
    extendedDynamicState2Features.extendedDynamicState2 = VK_TRUE;

    window->setEnabledFeaturesModifier(+[](VkPhysicalDeviceFeatures2& features2) {
        features2.pNext  = &features12;
        features12.pNext = &features13;
        features13.pNext = &features14;
        features14.pNext = &extendedDynamicState2Features;
    });
}
