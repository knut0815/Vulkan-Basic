#include "deleter.h"

// vk headers
#include "vulkan.h"

// glfw headers
#include "glfw3.h"

// glm headers
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/vec4.hpp"
#include "glm/mat4x4.hpp"

// other
#include <iostream>
#include <stdexcept>
#include <vector>
#include <set>
#include <algorithm>

class BasicApp
{

public:

void run()
{
	initWindow();
	initVulkan();
	mainLoop();
}

~BasicApp()
{
	glfwDestroyWindow(mWindow);
	glfwTerminate();
}

private:

	void initWindow()
	{
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		mWindow = glfwCreateWindow(mWidth, mHeight, "Vulkan App", nullptr, nullptr);
	}

	void initVulkan()
	{
		createInstance();
		setupDebugCallback();
		createSurface();
		pickPhysicalDevice();
		createLogicalDevice();
		createSwapChain();
	}

	void mainLoop()
	{
		while (!glfwWindowShouldClose(mWindow))
		{
			glfwPollEvents();
		}
	}

	//! a struct for determining which queue families a physical device supports
	struct QueueFamilyIndices
	{
		int graphicsFamily = -1;
		int presentFamily = -1;
		bool isComplete() { return graphicsFamily >= 0 && presentFamily >= 0; }
	};

	//! a struct for determining whether a swap chain is compatible with the window surface
	struct SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

	//! create an instance, which includes global extensions and validation layers
	void createInstance()
	{
		if (mEnableValidationLayers && !checkValidationLayerSupport())
		{
			throw std::runtime_error("One or more validation layers specified by this application are not supported.");
		}
		
		/*

		In Vulkan, an instance is the connection between your application and the Vulkan library
		and creating it involves specifying some details about your application to the driver.
		To create an instance, we first fill in a struct with some information about our application.
		This step is technically optional, but it may provide some useful information to the driver
		to optimize for our specific application.

		A lot of information in Vulkan is passed through structs instead of function parameters.

		The VkInstanceCreateInfo struct is not optional - it tells the Vulkan driver which global
		extensions and validation layers we want to use. Here, "global" means that they apply to the
		entire program and not a specific device.

		*/

		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Hello Triangle";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		/*

		The next two members of the VkInstanceCreateInfo struct specify the desired global
		extensions. Since Vulkan is platform agnostic, we need a special extension to interface with
		the windowing system. GLFW has a built-in function that returns the extension(s) it needs to
		do so.

		The last member (enabledLayerCount) of the struct determines the global validation layers to enable. 

		*/

		auto extensions = getRequiredExtensions();
		createInfo.enabledExtensionCount = extensions.size();
		createInfo.ppEnabledExtensionNames = extensions.data();

		if (mEnableValidationLayers)
		{
			createInfo.enabledLayerCount = mValidationLayers.size();
			createInfo.ppEnabledLayerNames = mValidationLayers.data();
		}
		else
		{
			createInfo.enabledLayerCount = 0;
		}

		/*

		We can now create our VkInstance. The general pattern that object creation function parameters
		in Vulkan follow is:

			1. Pointer to struct with creation info
			2. Pointer to custom allocator callbacks (always nullptr in this application)
			3. Pointer to the variable that stores the handle to the new object

		*/

		if(vkCreateInstance(&createInfo, nullptr, &mInstance) != VK_SUCCESS) // returns a VkResult
		{
			throw std::runtime_error("Failed to create VkInstance.");
		}

	}

	//! check if all of the requested validation layers are installed on this system
	bool checkValidationLayerSupport()
	{
		/*
		
		The Vulkan API is designed around the idea of minimal driver overhead. As a result, there is very 
		limited error checking enabled by default. Validation layers are optional components that hook into
		Vulkan function calls to apply additional operations (i.e. checking the values of function parameters
		against the specification to detect misuse, tracking the creation and destruction of objects to find
		resource leaks, etc.).

		Vulkan does not come with any validation layers built-in, but the LunarG SDK provides a nice set of
		layers that will check common errors. Validation layers can only be used if they are installed on
		the system. A layer is enabled by specifying its name.

		We want to check if all of the requested validation layers are available on this system. To do
		this, we call vkEnumerateInstanceLayerProperties.

		*/

		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : mValidationLayers)
		{
			auto predicate = [&](const VkLayerProperties &prop) { return strcmp(layerName, prop.layerName); };
			if (std::find_if(availableLayers.begin(), availableLayers.end(), predicate) == availableLayers.end())
			{
				return false;
			}
		}
		return true;
	}

	//! retrieve the list of extensions required by GLFW
	std::vector<const char*> getRequiredExtensions()
	{
		/*
		
		Get the list of extensions that are required by GLFW. We optionally add the VK_EXT_debug_report extension,
		which lets us receive debug messages from Vulkan.

		*/

		std::vector<const char*> extensions;
		unsigned int glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::cout << "Adding extensions required by GLFW:" << std::endl;

		for (unsigned int i = 0; i < glfwExtensionCount; ++i)
		{
			std::cout << "\t" << glfwExtensions[i] << std::endl;
			extensions.push_back(glfwExtensions[i]);
		}

		if (mEnableValidationLayers)
		{
			extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		}

		return extensions;
	}

	//! retrieve the list of all supported extensions on this system
	std::vector<VkExtensionProperties> getAvailableExtensions()
	{
		/*

		We might want to check for optional (non-essential) extensions. To retrieve a list of supported
		extensions before creating an instance, we call vkEnumerateInstanceExtensionProperties, which
		takes a pointer to a variable that stores the number of extensions and an array of VkExtensionProperties
		to store the details of the extensions. It also takes an optional first parameter that allows
		us to filter extensions by a specific validation layer, which we ignore for now.

		*/

		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr); // find number of supported extensions

		std::vector<VkExtensionProperties> extensions(extensionCount);

		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data()); // fill vector with extension data

		return extensions;
	}

	//! creates a callback object
	void setupDebugCallback()
	{	
		/*
		
		Even a debug callback in Vulkan is managed by a handle that needs to be explicitly created and 
		destroyed. First, we need to fill in a structure with details about the callback.

		The flags field allows you to filter which types of messages you would like to receive. The pfnCallback
		field specifies the pointer to the callback function. You can optionally pass a pointer to the pUserData
		field which will be passed along to the callback function via the userData parameter. You could use 
		this to pass a pointer to the application class, for example.

		This struct should be passed to the vkCreateDebugReportCallbackEXT function to create the VkDebugReportCallbackEXT
		object. Unfortunately, because this function is an extension function, it is not automatically loaded. We 
		have to look up its address ourselves using vkGetInstanceProcAddr. We use our own proxy function called
		CreateDebugReportCallbackEXT that handles this in the background.

		*/

		if (!mEnableValidationLayers) return;

		VkDebugReportCallbackCreateInfoEXT createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
		createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
		createInfo.pfnCallback = (PFN_vkDebugReportCallbackEXT)sDebugCallback;

		if (CreateDebugReportCallbackEXT(mInstance, &createInfo, nullptr, &mCallback) != VK_SUCCESS) // calls CreateDebugReportCallbackEXT
		{
			throw std::runtime_error("Failed to setup debug callback.");
		}
	}

	//! explicitly loads a function from the Vulkan extension for creating a VkDebugReportCallbackEXT object
	static VkResult CreateDebugReportCallbackEXT(
		VkInstance instance,
		const VkDebugReportCallbackCreateInfoEXT* pCreateInfo,
		const VkAllocationCallbacks* pAllocator,
		VkDebugReportCallbackEXT* pCallback)
	{
		auto func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
		if (func != nullptr)
		{
			return func(instance, pCreateInfo, pAllocator, pCallback);
		}
		else
		{
			return VK_ERROR_EXTENSION_NOT_PRESENT;
		}
	}

	//! explicitly loads a function from the Vulkan extension for cleaning up a VkDebugReportCallbackEXT object
	static void DestroyDebugReportCallbackEXT(
		VkInstance instance,
		VkDebugReportCallbackEXT callback,
		const VkAllocationCallbacks* pAllocator
		)
	{
		auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
		if (func != nullptr)
		{
			func(instance, callback, pAllocator);
		}
	}

	//! create a window surface to render to
	void createSurface()
	{
		/*
		
		To establish a connection between Vulkan and the window system to present the results to the screen,
		we need to use the WSI (Window System Integration) extensions, the first of which is VK_KHR_surface.
		This extension exposes the VkSurfaceKHR object, which represents an abstract type of surface to
		present rendered images to. The surface in our program will be backed by the window that we've already
		opened with GLFW. Although the VkSurfaceKHR object and its usage is platform agnostic, its creation
		is not because it depends on the underlying window system's details. For example, it needs the HWND
		and HMODULE handles on Windows. Luckily, GLFW does most of this for us.

		The VK_KHR_surface extension is an instance level extesion, which we've actually already enabled
		via the getRequiredExtensions function. The window surface needs to be created right after the 
		instance creation because it can actually influence the physical device selection process.

		It should be noted that window surfaces are an entirely optional component in Vulkan (i.e. we might
		just need off-screen rendering). We can do this very easily and without any "hacks," which was 
		necessary in OpenGL.

		Although the Vulkan implementation may support window system integration, that does not mean that 
		every device in the system supports it. There, we extend the isDeviceSuitable function to ensure
		that a device can present images to the surface we create.
		
		*/
		
		if (glfwCreateWindowSurface(mInstance, mWindow, nullptr, &mSurface) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create window surface.");
		}
		
	}

	//! choose a physical device (GPU) to use for rendering
	void pickPhysicalDevice()
	{
		/*
		
		We need to look for and select a graphics card in the system that supports all of the features
		we need for rendering. We can select more than one graphics card, but in this example, we'll
		stick to the first one found.

		We call vkEnumeratePhysicalDevices twice: once to count the number of available devices (GPUs) 
		and once to fill a vector with all available VkPhysicalDevice handles. Then, we loop through
		each device to see if it is suitable for the operations we want to perform, since not all 
		graphics cards are created equal!

		*/

		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(mInstance, &deviceCount, nullptr);

		if (deviceCount == 0)
		{
			throw std::runtime_error("Failed to find GPUs with Vulkan support.");
		}

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(mInstance, &deviceCount, devices.data());

		for (const auto& device : devices)
		{
			if (isDeviceSuitable(device))
			{
				mPhysicalDevice = device; // simply select the first suitable device
				break;
			}
		}

		if (mPhysicalDevice == VK_NULL_HANDLE)
		{
			throw std::runtime_error("Failed to find a suitable GPU.");
		}
	}

	//! check if all requested device extensions are supported
	bool checkDeviceExtensionSupport(VkPhysicalDevice device)
	{
		/*

		In Vulkan the swap chain is the infrastructure that gives you images to render to that can be
		presented to the screen afterwards. It is essentially a queue of images that are waiting to
		be presented to the screen. Our application will acquire an image to draw to and then return
		it to the queue. The general purpose of the swap chain is to synchronize the presentation of
		images with the refresh rate of the screen.

		Not all graphics cards are capable of presenting images directly to a screen for various reasons
		(i.e. a GPU that is designed for servers that don't have any display outputs). Since image
		presentation is heavily tied into the window system and the surfaces associated with windows,
		it is not actually part of the Vulkan core. This functionality exists as part of the VK_KHR_swapchain
		device extension.

		*/

		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		std::set<std::string> requiredExtensions(mDeviceExtensions.begin(), mDeviceExtensions.end());

		std::cout << "Available extensions: " << std::endl;
		for (const auto& extension : availableExtensions)
		{
			std::cout << "\t" << extension.extensionName << std::endl;

			// check off (erase) each extension that we know is available
			requiredExtensions.erase(extension.extensionName);
		}

		// if requiredExtensions is empty, that means we have support for all of the requested extensions
		return requiredExtensions.empty();
	}

	//! given a physical device, determine whether it is suitable for rendering (mostly a placeholder at this point)
	bool isDeviceSuitable(VkPhysicalDevice device)
	{
		/*
		
		Basic device properties (name, type, and supported Vulkan version) can be queried using
		vkGetPhysicalDeviceProperties. The support for optional features (i.e. texture compression,
		64-bit floats, multi-viewport rendering for VR, etc.) can be queried with vkGetPhysicalDeviceFeatures.
		
		In a more advanced application, we could give each device a score and pick the highest one.
		That way we could favor a dedicated graphics card by giving it a high score. For now, we return 
		true if this device supports the graphics queue family and it has all of the requested device
		extensions (i.e. VK_KHR_swapchain).
		
		Examples of more advanced features: 

		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);
		
		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

		if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && deviceFeatures.geometryShader)
		{
			std::cout << "Geometry shaders are supported on this graphics card." << std::endl;
		}

		*/

		// make sure this device supports the graphics queue family
		QueueFamilyIndices indices = findQueueFamilies(device);

		// make sure that the requested device extensions are supported
		bool extensionsSupported = checkDeviceExtensionSupport(device);

		// make sure that the swap chain is adequate
		bool swapChainAdequate = false;
		if (extensionsSupported)
		{
			// for our purposes, a swap chain is adequate if there is at least one supported image format and one supported presentation mode given our surface
			SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
			swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
		}

		return indices.isComplete() && extensionsSupported && swapChainAdequate;
	}

	//! determines which queue families the specified physical device supports
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device)
	{
		/*
		
		Commands in Vulkan need to be submitted to a queue. There are different types of queues that 
		originate from different queue families, and each family of queues allows only a certain subset
		of commands. For example, there could be a queue family that only allows processing of compute
		commands or one that only allows memory transfer related commands.

		We need to check which queue families are supported by the device and which one of these supports
		the commands that we want to use. Right now, we'll only look for a queue that supports graphics
		commands. We also need to find a queue family that has the capability of presenting to our window
		surface (see the notes in createSurface).

		The VkQueueFamilyProperties struct contains some details about the queue family, including the
		type of operations that are supported and the number of queues that can be created based on that
		family.
			
		*/

		QueueFamilyIndices indices;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		int i = 0;
		for (const auto& queueFamily : queueFamilies)
		{
			// check for graphics support
			if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				indices.graphicsFamily = i;
			}

			// check for present support
			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, mSurface, &presentSupport);

			if (queueFamily.queueCount > 0 && presentSupport)
			{
				indices.presentFamily = i;
			}

			if (indices.isComplete())
			{
				break;
			}
			i++;
		}

		return indices;
	}

	//! creates a logical device
	void createLogicalDevice()
	{
		/*
		
		The creation of a logical device involves specifying a bunch of details in structs. VkDeviceQueueCreateInfo
		describes the number of queues we want for a single queue family. Right now we're only interested
		in two queues: one with graphics capabilities and one with presentation capabilities.

		Current drivers will only allow you to create a low number of queues within each queue family, but
		you don't really need more than one because you can create all of the command buffers on multiple
		threads and then submit them all at once on the main thread with a single low-overhead call. Vulkan
		also lets you assign priorities to queues to influence the scheduling of command buffer execution using
		floating point numbers between 0.0f and 1.0f. This is required even if there is only a single queue.
		
		Next, we need to specify the set of device features that we'll be using. These are the features
		that we queried support for with vkGetPhysicalDeviceFeatures. Right now, we can simply define it and
		leave everything to VK_FALSE.

		The rest of the VkDeviceCreateInfo struct looks similar to the VkInstanceCreateInfo struct and 
		requires you to specify extensions and validation layers. This time, however, they are device
		specific. An example of a device specific extension is VK_KHR_swapchain, which allows you to 
		present rendered images from that device to windows. It is possible that there are Vulkan devices 
		in the system that lack this ability, for example because they only support compute operations.

		*/

		QueueFamilyIndices indices = findQueueFamilies(mPhysicalDevice);

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<int> uniqueQueueFamilies = { indices.graphicsFamily, indices.presentFamily };

		for (int queueFamily : uniqueQueueFamilies)
		{
			float queuePriority = 1.0f;

			// setup the VkDeviceQueueCreateInfo struct for each queue family
			VkDeviceQueueCreateInfo queueCreateInfo = {};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = indices.graphicsFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;

			// push back
			queueCreateInfos.push_back(queueCreateInfo);
		}

		VkPhysicalDeviceFeatures deviceFeatures = {};

		VkDeviceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
		createInfo.pEnabledFeatures = &deviceFeatures;
		
		// enable the request extensions (we check that they exist on this system in checkDeviceExtensionSupport)
		createInfo.enabledExtensionCount = mDeviceExtensions.size();
		createInfo.ppEnabledExtensionNames = mDeviceExtensions.data();
		
		if (mEnableValidationLayers)
		{
			createInfo.enabledLayerCount = mValidationLayers.size();
			createInfo.ppEnabledLayerNames = mValidationLayers.data();
		}
		else
		{
			createInfo.enabledLayerCount = 0;
		}

		if (vkCreateDevice(mPhysicalDevice, &createInfo, nullptr, &mDevice) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create logical device.");
		}

		// pass the logical device, queue family, queue index, and a pointer to the variable where we'll store the queue handle
		// the index is 0 because we are only using one queue from each family
		vkGetDeviceQueue(mDevice, indices.graphicsFamily, 0, &mGraphicsQueue);
		vkGetDeviceQueue(mDevice, indices.presentFamily, 0, &mPresentQueue);
	}

	//! checks whether the swap chain is compatible with our window surface
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device)
	{
		/*
		
		Just checking if a swap chain is available is not sufficient because it may not actually be
		compatible with our window surface. We want to check 3 properties:

		1. Basic surface capabilities (i.e. min/max number of images in the swap chain, min/max width and height, etc.)
		2. Surface formats (i.e. pixel format and color space)
		3. Available presentation modes

		*/

		SwapChainSupportDetails details;

		// 1.
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, mSurface, &details.capabilities);

		// 2.
		uint32_t formatCount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, mSurface, &formatCount, nullptr);

		if (formatCount != 0)
		{
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, mSurface, &formatCount, details.formats.data());
		}

		// 3.
		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, mSurface, &presentModeCount, nullptr);

		if (presentModeCount != 0)
		{
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, mSurface, &presentModeCount, details.presentModes.data());
		}

		// pass to isDeviceSuitable

		return details;
	}

	//! choose the best surface format for the swap chain
	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
	{
		/*
		
		We want to find the right settings for the best possible swap chain. There are three types of settings 
		to determine:

		1. Surface format (color depth)
		2. Presentation mode (conditions for "swapping" images to the screen)
		3. Swap extent (resolution of images in the swap chain

		This function will receive the formats member of the SwapChainSupportDetails struct as an argument.
		Each VkSurfaceFormatKHR entry contains a format and colorSpace member. The format member specifies 
		the color channels and types. For example, VK_FORMAT_B8G8R8A8_UNORM means that we store the B, G, R,
		and alpha channels in that order with an 8-bit unsigned integer for a total of 32-bits per pixel. The
		colorSpace member indicates if the SRGB color space is supported or not using the VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
		flag.

		For the color space, we'll use SRGB if it is available because it results in more accurate perceived 
		colors. For the format, we'll use the most common: VK_FORMAT_B8G8R8A8_UNORM. 

		The best case scenario is that the surface has no preferred format, which Vulkan indicates by only 
		returning one VkSurfaceFormatKHR entry which has its format member set to VK_FORMAT_UNDEFINED.

		*/

		// no preferred format
		if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED)
		{
			std::cout << "No preferred surface format: defaulting to VK_FORMAT_B8G8R8A8_UNORM and VK_COLOR_SPACE_SRGB_NONLINEAR_KHR." << std::endl;
			return { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
		}

		// otherwise, go through the list and see if the preferred combination is available
		for (const auto& availableFormat : availableFormats)
		{
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				std::cout << "Found an available surface format with VK_FORMAT_B8G8R8A8_UNORM and VK_COLOR_SPACE_SRGB_NONLINEAR_KHR." << std::endl;
				return availableFormat;
			}
		}

		// if that also fails, we could start ranking the available formats based on some metric, but in most cases, it's okay to just settle with the first format that is specified
		std::cout << "No surface format found with the preferred settings. Returning the first available format." << std::endl;
		return availableFormats[0];
	}

	//! choose the best presentation mode for the swap chain
	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
	{
		/*
		
		The presentation mode is arguably the most important setting for the swap chain because it 
		represents the actual conditions for showing images to the screen. The four possible modes are:

		1. VK_PRESENT_MODE_IMMEDIATE_KHR: images submitted are transferred to the screen right away, 
		   which may result in tearing
		2. VK_PRESENT_MODE_FIFO_KHR: the swap chain is a queue where the display takes an image from the
		   front of the queue on a vertical blank and the program inserts rendered images at the back of
		   the queue - this is most similar to vertical sync found in modern games
		3. VK_PRESENT_MODE_FIFO_RELAXED_KHR: this mode differs from the first when the application is 
		   late and the queue was empty at the last vertical blank - instead of waiting for the next
		   vertical blank, the image is transferred right away when it finally arrives, which may result
		   in tearing
		4. VK_PRESENT_MODE_MAILBOX_KHR: this is another variation of the first mode - instead of blocking
		   the application when the queue is full, the images that are already queued are simply replaced
		   with the newer ones (you can use this to implement triple buffering)

		Only 2 is guaranteed to be available.
		
		*/

		for (const auto& availablePresentMode : availablePresentModes)
		{
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				return availablePresentMode;
			}
		}

		return VK_PRESENT_MODE_FIFO_KHR;
	}

	//! choose the extent (resolution) of the swap chain images
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
	{
		/*
		
		The swap extent is the resolution of the swap chain images and it's almost always exactly equal
		to the resolution of the window that we're drawing to. The range of possible resolutions is defined
		in the VkSurfaceCapabilitiesKHR structure. Vulkan tells us to match the resolution of the window by 
		setting the width and height in the currentExtent member.

		However, some window managers do allow us to differ here, and this is indicated by setting the width
		and height in currentExtent to the maximum value of a uint32_t. In that case, we'll pick the resolution
		that best matches the window within the minImageExtent and maxImageExtent bounds.
		
		*/

		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			return capabilities.currentExtent;
		}
		else
		{
			VkExtent2D actualExtent = { mWidth, mHeight };
			actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
			actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));
			return actualExtent;
		}
	}

	//! create a swap chain
	void createSwapChain()
	{
		/*
		
		We need to create the swap chain using our helper functions above. We also need to specify the number 
		of images in the swap chain (essentially the queue length). The implementation specifies the minimum 
		amount of images to function properly and we'll try to have one more than that to properly implement 
		triple buffering.
		
		We need to specify how to handle swap chain images that will be used across multiple queue families.
		That will be the case in our application if the graphics queue family is different from the presentation
		queue. We'll be drawing on the images in the swap chain from the graphics queue and then submitting them
		on the presentation queue. There are two ways to handle images that are accessed from multiple queues:

		1. VK_SHARING_MODE_CONCURRENT: images can be used across multiple queue families without explicit 
		   ownership transfers
		2. VK_SHARING_MODE_EXCLUSIVE: an image is owned by one queue family at a time and ownership must be
		   explicitly transferred before using it in another queue family - this offers better performance

		If the queue families differ, then we'll be using the concurrent mode in this tutorial to avoid having
		to do the ownership chapters. Concurrent mode requires you to specify in advance between which queue
		families ownership will be shared using the queueFamilyIndexCount and pQueueFamilyIndices parameters.
		If the graphics queue family and presentation queue family are the same, which will be the case on 
		most hardware, then we should stick to exclusive mode because concurrent mode requires you to specify
		at least two distinct queue families.

		*/

		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(mPhysicalDevice);

		auto surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
		auto presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
		auto extent = chooseSwapExtent(swapChainSupport.capabilities);

		// a value of 0 for maxImageCount means that there is no limit besides memory requirements, which is why we have this check
		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
		if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
		{
			imageCount = swapChainSupport.capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = mSurface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1; // always 1 unless you're developing a stereoscopic application
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // what kind of operations we'll use the images for (in this case, direct rendering)

		QueueFamilyIndices indices = findQueueFamilies(mPhysicalDevice);
		uint32_t queueFamilyIndices[] = { (uint32_t)indices.graphicsFamily, (uint32_t)indices.presentFamily };
		if (indices.graphicsFamily != indices.presentFamily)
		{
			std::cout << "Settings swap chain share mode to: VK_SHARING_MODE_CONCURRENT." << std::endl;
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else
		{
			std::cout << "Settings swap chain share mode to: VK_SHARING_MODE_EXCLUSIVE." << std::endl;
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0; // optional
			createInfo.pQueueFamilyIndices = nullptr; // optional
		}

		createInfo.preTransform = swapChainSupport.capabilities.currentTransform; // 90 degree clockwise rotation, etc.
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // specifies if the alpha channel should be used for blending with other windows in the system
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE; // we don't care about the color of pixels that are obscured (behind another window)
		createInfo.oldSwapchain = VK_NULL_HANDLE; // complex, don't worry about it (for multiple swap chains)

		if (vkCreateSwapchainKHR(mDevice, &createInfo, nullptr, &mSwapChain) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create swap chain.");
		}
	}

	GLFWwindow *mWindow;
	const int mWidth = 800;
	const int mHeight = 600;
	vk::Deleter<VkInstance> mInstance{ vkDestroyInstance };
	vk::Deleter<VkDebugReportCallbackEXT> mCallback{ mInstance, DestroyDebugReportCallbackEXT };
	vk::Deleter<VkSurfaceKHR> mSurface{ mInstance, vkDestroySurfaceKHR };
	VkPhysicalDevice mPhysicalDevice{ VK_NULL_HANDLE };	// implicitly destroyed when the VkInstance is destroyed, so we don't need to add a delete wrapper
	vk::Deleter<VkDevice> mDevice{ vkDestroyDevice }; // needs to be declared below the VkInstance, since it must be destroyed before the instance is cleaned up
	VkQueue mGraphicsQueue; // automatically created and destroyed alongside the logical device
	VkQueue mPresentQueue;
	vk::Deleter<VkSwapchainKHR> mSwapChain{ mDevice, vkDestroySwapchainKHR };

	const std::vector<const char*> mDeviceExtensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	static VkBool32 sDebugCallback (
		VkDebugReportFlagsEXT flags,			// type of the message, i.e. VK_DEBUG_REPORT_INFORMATION_BIT_EXT, VK_DEBUG_REPORT_WARNING_BIT_EXT, etc.
		VkDebugReportObjectTypeEXT objectType,	// type of the object that is the subject of the message, i.e. VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_EXT if object is a VkPhysicalDevice
		uint64_t object,						// handle to the actual Vulkan object, i.e. a VkPhysicalDevice
		size_t location,
		int32_t code,
		const char* layerPrefix,
		const char* messsage,					// the debug message
		void* userData)							// you can pass your own userData to the callback
	{
		std::cerr << "Validation layer: " << messsage << std::endl;
		return VK_FALSE;
	}

	const std::vector<const char*> mValidationLayers{ "VK_LAYER_LUNARG_standard_validation" }; // only active if we're in debug mode
	
#ifdef NDEBUG
	const bool mEnableValidationLayers = false;
#else
	const bool mEnableValidationLayers = true;
#endif

};

int main()
{
	BasicApp app;

	try
	{
		app.run();
	} 
	catch (const std::runtime_error &e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;

	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
	std::cout << extensionCount << " extensions supported." << std::endl;

	return 0;
}	