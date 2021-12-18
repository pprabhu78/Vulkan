#include "Instance.h"
#include "VulkanDebug.h"
#include <iostream>
#include <sstream>

namespace genesis
{
   PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT;
   PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;
   VkDebugUtilsMessengerEXT debugUtilsMessenger;

   VKAPI_ATTR VkBool32 VKAPI_CALL debugUtilsMessengerCallback(
      VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
      VkDebugUtilsMessageTypeFlagsEXT messageType,
      const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
      void* pUserData)
   {
      // Select prefix depending on flags passed to the callback
      std::string prefix("");

      if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
         prefix = "VERBOSE: ";
      }
      else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
         prefix = "INFO: ";
      }
      else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
         prefix = "WARNING: ";
      }
      else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
         prefix = "ERROR: ";
      }

      // Display message to default output (console/logcat)
      std::stringstream debugMessage;

      debugMessage << "messageIdNumber: " << pCallbackData->messageIdNumber << std::endl;
      debugMessage << "pMessageIdName: " << std::string(pCallbackData->pMessageIdName) << std::endl;
      debugMessage << "message: " << std::endl;
      debugMessage << pCallbackData->pMessage << std::endl;


#if defined(__ANDROID__)
      if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
         LOGE("%s", debugMessage.str().c_str());
      }
      else {
         LOGD("%s", debugMessage.str().c_str());
      }
#else
      if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
         std::cout << debugMessage.str() << "\n";
      }
      else {
         std::cout << debugMessage.str() << "\n";
      }
      fflush(stdout);
#endif

      // The return value of this callback controls whether the Vulkan call that caused the validation message will be aborted or not
      // We return VK_FALSE as we DON'T want Vulkan calls that cause a validation message to abort
      // If you instead want to have calls abort, pass in VK_TRUE and the function will return VK_ERROR_VALIDATION_FAILED_EXT 
      return VK_FALSE;
   }

   void setupDebugging(VkInstance instance, VkDebugReportFlagsEXT flags, VkDebugReportCallbackEXT callBack)
   {
      vkCreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
      vkDestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));

      VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCI{};
      debugUtilsMessengerCI.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
      debugUtilsMessengerCI.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
      debugUtilsMessengerCI.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
      debugUtilsMessengerCI.pfnUserCallback = debugUtilsMessengerCallback;
      VkResult result = vkCreateDebugUtilsMessengerEXT(instance, &debugUtilsMessengerCI, nullptr, &debugUtilsMessenger);
      assert(result == VK_SUCCESS);
   }

   void freeDebugCallback(VkInstance instance)
   {
      if (debugUtilsMessenger != VK_NULL_HANDLE)
      {
         vkDestroyDebugUtilsMessengerEXT(instance, debugUtilsMessenger, nullptr);
      }
   }

   Instance::Instance(const std::string& name, std::vector<std::string>& instanceExtensionsToEnable, uint32_t apiVersion, bool validation)
      : _validation(validation)
      , _createResult(VK_RESULT_MAX_ENUM)
      , _instance(0)
   {

      VkApplicationInfo appInfo = {};
      appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
      appInfo.pApplicationName = name.c_str();
      appInfo.pEngineName = name.c_str();
      appInfo.apiVersion = apiVersion;

      std::vector<const char*> instanceExtensions = { VK_KHR_SURFACE_EXTENSION_NAME };

      // Enable surface extensions depending on os
#if defined(_WIN32)
      instanceExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
      instanceExtensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#elif defined(_DIRECT2DISPLAY)
      instanceExtensions.push_back(VK_KHR_DISPLAY_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_DIRECTFB_EXT)
      instanceExtensions.push_back(VK_EXT_DIRECTFB_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
      instanceExtensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
      instanceExtensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_IOS_MVK)
      instanceExtensions.push_back(VK_MVK_IOS_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
      instanceExtensions.push_back(VK_MVK_MACOS_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_HEADLESS_EXT)
      instanceExtensions.push_back(VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME);
#endif

      // Get extensions supported by the instance and store for later use
      uint32_t extCount = 0;
      vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
      if (extCount > 0)
      {
         std::vector<VkExtensionProperties> extensions(extCount);
         if (vkEnumerateInstanceExtensionProperties(nullptr, &extCount, &extensions.front()) == VK_SUCCESS)
         {
            for (VkExtensionProperties extension : extensions)
            {
               _supportedInstanceExtensions.push_back(extension.extensionName);
            }
         }
      }

      // Enabled requested instance extensions
      if (instanceExtensionsToEnable.size() > 0)
      {
         for (const std::string& enabledExtension : instanceExtensionsToEnable)
         {
            // Output message if requested extension is not available
            if (std::find(_supportedInstanceExtensions.begin(), _supportedInstanceExtensions.end(), enabledExtension) == _supportedInstanceExtensions.end())
            {
               std::cerr << "Enabled instance extension \"" << enabledExtension << "\" is not present at instance level\n";
            }
            instanceExtensions.push_back(enabledExtension.c_str());
         }
      }

      VkInstanceCreateInfo instanceCreateInfo = {};
      instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
      instanceCreateInfo.pNext = NULL;
      instanceCreateInfo.pApplicationInfo = &appInfo;
      if (instanceExtensions.size() > 0)
      {
         if (_validation)
         {
            instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
         }
         instanceCreateInfo.enabledExtensionCount = (uint32_t)instanceExtensions.size();
         instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
      }

      // The VK_LAYER_KHRONOS_validation contains all current validation functionality.
      // Note that on Android this layer requires at least NDK r20
      const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
      if (_validation)
      {
         // Check if this layer is available at instance level
         uint32_t instanceLayerCount;
         vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
         std::vector<VkLayerProperties> instanceLayerProperties(instanceLayerCount);
         vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayerProperties.data());
         bool validationLayerPresent = false;
         for (VkLayerProperties layer : instanceLayerProperties) {
            if (strcmp(layer.layerName, validationLayerName) == 0) {
               validationLayerPresent = true;
               break;
            }
         }
         if (validationLayerPresent) {
            instanceCreateInfo.ppEnabledLayerNames = &validationLayerName;
            instanceCreateInfo.enabledLayerCount = 1;
         }
         else {
            std::cerr << "Validation layer VK_LAYER_KHRONOS_validation not present, validation is disabled";
         }
      }
      _createResult = vkCreateInstance(&instanceCreateInfo, nullptr, &_instance);

      // The report flags determine what type of messages for the layers will be displayed
      // For validating (debugging) an application the error and warning bits should suffice
      VkDebugReportFlagsEXT debugReportFlags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
      // Additional flags include performance info, loader and layer debug messages, etc.
      setupDebugging(_instance, debugReportFlags, VK_NULL_HANDLE);

   }

   Instance::~Instance()
   {
      if (_validation)
      {
         freeDebugCallback(_instance);
      }

      vkDestroyInstance(_instance, nullptr);
   }

   VkInstance Instance::vulkanInstance(void) const
   {
      return _instance;
   }

   VkResult Instance::creationStatus(void) const
   {
      return _createResult;
   }

   bool Instance::enumeratePhysicalDevices(std::vector<int>& deviceIndices)
   {	
      // Physical device
      uint32_t gpuCount = 0;
      // Get number of available physical devices
      VK_CHECK_RESULT(vkEnumeratePhysicalDevices(_instance, &gpuCount, nullptr));
      if (gpuCount == 0) {
         std::cout << "No device with Vulkan support found" << std::endl;
         return false;
      }
      // Enumerate devices
      _physicalDevices.resize(gpuCount);
      VkResult err = vkEnumeratePhysicalDevices(_instance, &gpuCount, _physicalDevices.data());
      if (err) {
         std::cout << "Could not enumerate physical devices : \n" <<VulkanErrorToString::toString(err) << std::endl;
         return false;
      }
      return true;
   }

   const std::vector<VkPhysicalDevice>& Instance::physicalDevices(void) const
   {
      return _physicalDevices;
   }
}