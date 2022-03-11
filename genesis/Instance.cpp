#include "Instance.h"
#include "VulkanDebug.h"
#include <iostream>
#include <sstream>

namespace genesis
{
   PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT;
   PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;
   VkDebugUtilsMessengerEXT debugUtilsMessenger;

   static const std::string toString(VkObjectType type)
   {
      if (type == VK_OBJECT_TYPE_UNKNOWN)      return "VK_OBJECT_TYPE_UNKNOWN";
      if (type == VK_OBJECT_TYPE_INSTANCE)      return "VK_OBJECT_TYPE_INSTANCE";
      if (type == VK_OBJECT_TYPE_PHYSICAL_DEVICE)      return "VK_OBJECT_TYPE_PHYSICAL_DEVICE";
      if (type == VK_OBJECT_TYPE_DEVICE)      return "VK_OBJECT_TYPE_DEVICE";
      if (type == VK_OBJECT_TYPE_QUEUE)      return "VK_OBJECT_TYPE_QUEUE";
      if (type == VK_OBJECT_TYPE_SEMAPHORE)      return "VK_OBJECT_TYPE_SEMAPHORE";
      if (type == VK_OBJECT_TYPE_COMMAND_BUFFER)      return "VK_OBJECT_TYPE_COMMAND_BUFFER";
      if (type == VK_OBJECT_TYPE_FENCE)      return "VK_OBJECT_TYPE_FENCE";
      if (type == VK_OBJECT_TYPE_DEVICE_MEMORY)      return "VK_OBJECT_TYPE_DEVICE_MEMORY";
      if (type == VK_OBJECT_TYPE_BUFFER)      return "VK_OBJECT_TYPE_BUFFER";
      if (type == VK_OBJECT_TYPE_IMAGE)      return "VK_OBJECT_TYPE_IMAGE";
      if (type == VK_OBJECT_TYPE_EVENT)      return "VK_OBJECT_TYPE_EVENT";
      if (type == VK_OBJECT_TYPE_QUERY_POOL)      return "VK_OBJECT_TYPE_QUERY_POOL";
      if (type == VK_OBJECT_TYPE_BUFFER_VIEW)      return "VK_OBJECT_TYPE_BUFFER_VIEW";
      if (type == VK_OBJECT_TYPE_IMAGE_VIEW)      return "VK_OBJECT_TYPE_IMAGE_VIEW";
      if (type == VK_OBJECT_TYPE_SHADER_MODULE)      return "VK_OBJECT_TYPE_SHADER_MODULE";
      if (type == VK_OBJECT_TYPE_PIPELINE_CACHE)      return "VK_OBJECT_TYPE_PIPELINE_CACHE";
      if (type == VK_OBJECT_TYPE_PIPELINE_LAYOUT)      return "VK_OBJECT_TYPE_PIPELINE_LAYOUT";
      if (type == VK_OBJECT_TYPE_RENDER_PASS)      return "VK_OBJECT_TYPE_RENDER_PASS";
      if (type == VK_OBJECT_TYPE_PIPELINE)      return "VK_OBJECT_TYPE_PIPELINE";
      if (type == VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT)      return "VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT";
      if (type == VK_OBJECT_TYPE_SAMPLER)      return "VK_OBJECT_TYPE_SAMPLER";
      if (type == VK_OBJECT_TYPE_DESCRIPTOR_POOL)      return "VK_OBJECT_TYPE_DESCRIPTOR_POOL";
      if (type == VK_OBJECT_TYPE_DESCRIPTOR_SET)      return "VK_OBJECT_TYPE_DESCRIPTOR_SET";
      if (type == VK_OBJECT_TYPE_FRAMEBUFFER)      return "VK_OBJECT_TYPE_FRAMEBUFFER";
      if (type == VK_OBJECT_TYPE_COMMAND_POOL)      return "VK_OBJECT_TYPE_COMMAND_POOL";
      if (type == VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION)      return "VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION";
      if (type == VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE)      return "VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE";
      if (type == VK_OBJECT_TYPE_SURFACE_KHR)      return "VK_OBJECT_TYPE_SURFACE_KHR";
      if (type == VK_OBJECT_TYPE_SWAPCHAIN_KHR)      return "VK_OBJECT_TYPE_SWAPCHAIN_KHR";
      if (type == VK_OBJECT_TYPE_DISPLAY_KHR)      return "VK_OBJECT_TYPE_DISPLAY_KHR";
      if (type == VK_OBJECT_TYPE_DISPLAY_MODE_KHR)      return "VK_OBJECT_TYPE_DISPLAY_MODE_KHR";
      if (type == VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT)      return "VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT";

      return "VK_OBJECT_TYPE_UNKNOWN";
   }
  
   VKAPI_ATTR VkBool32 VKAPI_CALL debugUtilsMessengerCallback(
      VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
      VkDebugUtilsMessageTypeFlagsEXT messageType,
      const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
      void* pUserData)
   {
      // Select prefix depending on flags passed to the callback
      std::string prefix("");

      if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
         std::cout << "VERBOSE: " << std::endl;
      }
      else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
         std::cout << "INFO: " << std::endl;
      }
      else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
         std::cout << "WARNING: " << std::endl;
      }
      else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
         std::cout << "ERROR: " << std::endl;
      }

      std::cout << "\t message id num : " << pCallbackData->messageIdNumber << std::endl;
      std::cout << "\t message id name: " << std::string(pCallbackData->pMessageIdName) << std::endl;
      std::cout << "\t message:" << std::string(pCallbackData->pMessageIdName) << std::endl;
      std::cout << "\t" << pCallbackData->pMessage << std::endl << std::endl;

      for (uint32_t i = 0; i < pCallbackData->objectCount; ++i)
      {
         VkDebugUtilsObjectNameInfoEXT object = pCallbackData->pObjects[i];
         if (object.pObjectName)
         {
            std::cout << "object[" << i << "]: " << object.pObjectName <<", " << toString(object.objectType) << std::endl;
         }
         else
         {
            std::cout << "object[" << i << "]: " << "unnamed" << ", " << toString(object.objectType) << std::endl;
         }  
      }

#if defined(__ANDROID__)
      if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
         LOGE("%s", debugMessage.str().c_str());
      }
      else {
         LOGD("%s", debugMessage.str().c_str());
      }
#else

      fflush(stdout);
#endif

      // The return value of this callback controls whether the Vulkan call that caused the validation message will be aborted or not
      // We return VK_FALSE as we DON'T want Vulkan calls that cause a validation message to abort
      // If you instead want to have calls abort, pass in VK_TRUE and the function will return VK_ERROR_VALIDATION_FAILED_EXT 
      return VK_FALSE;
   }

   void setupDebugging(VkInstance instance, VkDebugReportFlagsEXT flags, VkDebugReportCallbackEXT callBack)
   {
      bool nSightOn = false;
      // turn on debug groups if nsight is on
#if _WIN32
      HMODULE nsightModule = GetModuleHandle("Nvda.Graphics.Interception.dll");
      nSightOn = (nsightModule != 0);
#endif

#ifdef __linux__
      dl_iterate_phdr(findNsight, &nSightOn);
#endif
      // If nsight is on, don't install the callback
      if (nSightOn)
      {
         return;
      }

      vkCreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
      vkDestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));

      VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo{};
      debugUtilsMessengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
      debugUtilsMessengerCreateInfo.pNext = nullptr;
      debugUtilsMessengerCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
      debugUtilsMessengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
      debugUtilsMessengerCreateInfo.pfnUserCallback = debugUtilsMessengerCallback;
      VkResult result = vkCreateDebugUtilsMessengerEXT(instance, &debugUtilsMessengerCreateInfo, nullptr, &debugUtilsMessenger);
      assert(result == VK_SUCCESS);
   }

   void freeDebugCallback(VkInstance instance)
   {
      if (debugUtilsMessenger != VK_NULL_HANDLE)
      {
         vkDestroyDebugUtilsMessengerEXT(instance, debugUtilsMessenger, nullptr);
      }
   }

   ApiInstance::ApiInstance(const std::string& name, std::vector<std::string>& instanceExtensionsToEnable, uint32_t apiVersion, bool validation)
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
      if (_createResult != VK_SUCCESS)
      {
         std::cerr << __FUNCTION__ << "Could not create instance successfully, error:";
         std::cerr << "\t " << VulkanErrorToString::toString(_createResult) << std::endl;
         return;
      }

      // The report flags determine what type of messages for the layers will be displayed
      // For validating (debugging) an application the error and warning bits should suffice
      VkDebugReportFlagsEXT debugReportFlags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
      // Additional flags include performance info, loader and layer debug messages, etc.
      setupDebugging(_instance, debugReportFlags, VK_NULL_HANDLE);

   }

   ApiInstance::~ApiInstance()
   {
      if (_validation)
      {
         freeDebugCallback(_instance);
      }

      vkDestroyInstance(_instance, nullptr);
   }

   VkInstance ApiInstance::vulkanInstance(void) const
   {
      return _instance;
   }

   VkResult ApiInstance::creationStatus(void) const
   {
      return _createResult;
   }

   bool ApiInstance::enumeratePhysicalDevices()
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

   const std::vector<VkPhysicalDevice>& ApiInstance::physicalDevices(void) const
   {
      return _physicalDevices;
   }
}