#include "deleter.h"

// vk headers
#include "vulkan.h"

// glfw headers
#include "glfw3.h"

// glm headers
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

// stb headers
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// systen
#include <iostream>
#include <stdexcept>
#include <vector>
#include <set>
#include <array>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <chrono>

//! a struct for managing vertex data
struct Vertex
{
	glm::vec2 position;
	glm::vec3 color;
	glm::vec2 texcoord;

	//! a static function for returning the binding description of the specified vertex data
	static VkVertexInputBindingDescription getBindingDescription()
	{
		/*
		
		A VkVertexInputBindingDescription struct describes the rate at which the shader should load
		data from memory. It specifies the number of bytes between data entries and whether to move
		to the next data entry after each vertex or after each instance.
		
		*/

		VkVertexInputBindingDescription bindingDescription = {};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // move to the next data entry after each vertex
		return bindingDescription;
	}

	//! a static function for returning the attribute descriptions of the specified vertex data
	static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions()
	{
		/*
		
		Each VkVertexInputAttributeDescription struct describes how to extract a vertex attribute
		from a chunk of vertex data originating from a binding description. We need one for each
		attribute in our shader.
		
		*/

		std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions = {};

		// position
		attributeDescriptions[0].binding = 0;						// the binding that the per-vertex data comes from
		attributeDescriptions[0].location = 0;						// the location directive in the vertex shader
		attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;	// two floats 
		attributeDescriptions[0].offset = offsetof(Vertex, position);

		// color
		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, color);

		// texcoord
		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(Vertex, texcoord);

		return attributeDescriptions;
	}
};

struct UniformBufferObject
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 projection;
};

const std::vector<Vertex> vertices = {
	{ { -0.5f, -0.5f },	{ 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f }},
	{ { 0.5f, -0.5f },	{ 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f } },
	{ { 0.5f, 0.5f },	{ 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } },
	{ { -0.5f, 0.5f },	{ 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f } }
};

const std::vector<uint16_t> indices = {
	0, 1, 2, 2, 3, 0
};

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
		
		glfwSetWindowUserPointer(mWindow, this);
		glfwSetWindowSizeCallback(mWindow, BasicApp::resize);
	}

	static void resize(GLFWwindow* window, int width, int height)
	{
		/*
		
		GLFW's callback functions cannot be member functions. Luckily, we can store an arbitrary pointer
		in the window object with glfwSetWindowUserPointer, so we can specify a static class member and 
		get the original class instance back via the same function. 
		
		*/

		if (width == 0 || height == 0) return;
		BasicApp* app = reinterpret_cast<BasicApp*>(glfwGetWindowUserPointer(window));
		app->recreateSwapChain();
	}

	void initVulkan()
	{
		createInstance();
		setupDebugCallback();
		createSurface();
		pickPhysicalDevice();
		createLogicalDevice();
		createSwapChain();
		createImageViews();
		createRenderPass();
		createDescriptorSetLayout();
		createGraphicsPipeline();
		createFramebuffers();
		createCommandPool();
		createTextureImage();
		createTextureImageView();
		createTextureSampler();
		createVertexBuffer();
		createIndexBuffer();
		createUniformBuffer();
		createDescriptorPool();
		createDescriptorSet();
		createCommandBuffers();
		createSemaphores();
	}

	void mainLoop()
	{
		while (!glfwWindowShouldClose(mWindow))
		{
			glfwPollEvents();

			updateUniformBuffer();
			drawFrame();
		}

		// all operations in drawFrame are asynchronous, so we need to wait for the logical device to finish operations before cleaning up resources 
		vkDeviceWaitIdle(mDevice);
	}

	void updateUniformBuffer()
	{
		static auto startTime = std::chrono::high_resolution_clock::now();
		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count() / 1000.0f;

		UniformBufferObject ubo = {};
		ubo.model = glm::rotate(glm::mat4(), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		ubo.projection = glm::perspective(glm::radians(45.0f), mSwapChainExtent.width / static_cast<float>(mSwapChainExtent.height), 0.1f, 10.0f);
		ubo.projection[1][1] *= -1;

		void* data;
		vkMapMemory(mDevice, mUniformStagingBufferMemory, 0, sizeof(ubo), 0, &data);
		memcpy(data, &ubo, sizeof(ubo));
		vkUnmapMemory(mDevice, mUniformStagingBufferMemory);

		copyBuffer(mUniformStagingBuffer, mUniformBuffer, sizeof(ubo));
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

	//! create an instance, which is the connection between the application and the Vulkan library
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

		std::cout << "Successfully created intance object." << std::endl;
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

	//! creates a debug callback object
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

		if (CreateDebugReportCallbackEXT(mInstance, &createInfo, nullptr, &mCallback) != VK_SUCCESS) 
		{
			throw std::runtime_error("Failed to setup debug callback.");
		}

		std::cout << "Successfully created debug callback object." << std::endl;
	}

	//! explicitly loads a function from a Vulkan extension for creating a VkDebugReportCallbackEXT object
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

	//! explicitly loads a function from a Vulkan extension for destroying a VkDebugReportCallbackEXT object
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

	//! choose a physical device, which represents a GPU
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
			throw std::runtime_error("Failed to find a GPU with Vulkan support.");
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

		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(mPhysicalDevice, &deviceProperties);
		std::cout << "Sucessfully selected physical device: " << deviceProperties.deviceName << std::endl;
	}

	//! check if the specified physical device supports all of the requested extensions
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

	//! determine which queue families the specified physical device supports
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
		std::cout << "This physical device supports " << queueFamilyCount << " queue families." << std::endl;

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

	//! creates a logical device, which serves as an interface between the application and a physical device
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
		
		// enable the requested extensions (we check that they exist on this system in checkDeviceExtensionSupport)
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

		// no preferred format (i.e. the format is VK_FORMAT_UNDEFINED)
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

		Only VK_PRESENT_MODE_FIFO_KHR is guaranteed to be available.
		
		*/

		for (const auto& availablePresentMode : availablePresentModes)
		{
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				std::cout << "Found swap chain present mode: VK_PRESENT_MODE_MAILBOX_KHR." << std::endl;
				return availablePresentMode;
			}
		}

		std::cout << "Defaulting to swap chain present mode: VK_PRESENT_MODE_FIFO_KHR." << std::endl;
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

	//! create a swap chain from the chosen parameters 
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

		This function also properly handles recreating a swap chain when the window is resized.

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
		createInfo.imageArrayLayers = 1;								// always 1 unless you're developing a stereoscopic application
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;	// what kind of operations we'll use the images for (in this case, direct rendering)

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
			createInfo.queueFamilyIndexCount = 0;		// optional
			createInfo.pQueueFamilyIndices = nullptr;	// optional
		}

		// pass the previous swap chain object to the VkSwapchainCreateInfoKHR struct to indicate that we intend to replace it
		VkSwapchainKHR oldSwapChain = mSwapChain;

		createInfo.preTransform = swapChainSupport.capabilities.currentTransform;	// 90 degree clockwise rotation, etc.
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;				// specifies if the alpha channel should be used for blending with other windows in the system
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;												// we don't care about the color of pixels that are obscured (behind another window)
		createInfo.oldSwapchain = oldSwapChain; 

		// create the new swap chain
		VkSwapchainKHR newSwapChain;
		if (vkCreateSwapchainKHR(mDevice, &createInfo, nullptr, &newSwapChain) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create swap chain.");
		}
		*&mSwapChain = newSwapChain;

		std::cout << "Successfully created swap chain object with " << imageCount << " images." << std::endl;

		// retrieve the images from the swap chain and store them as a member variable
		vkGetSwapchainImagesKHR(mDevice, mSwapChain, &imageCount, nullptr);
		mSwapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(mDevice, mSwapChain, &imageCount, mSwapChainImages.data());

		// store the format and extent that we've chosen for the swap chain images as member variables
		mSwapChainImageFormat = surfaceFormat.format;
		mSwapChainExtent = extent;
	}

	//! create an array of image views from the swap chain images
	void createImageViews()
	{
		/*
		
		To use any VkImage objects, including those in the swap chain, we have to create a VkImageView.
		An image view is quite literally a view into an image. It describes how to access the image and
		which part of the image to access, for example if it should be treated as a 2D depth texture
		without any mipmapping levels.

		For now, we will create a basic image view for every image in the swap chain so that we can use
		them as color targets later on.
		
		*/
		
		// resize the list of image views and define the deleter function
		mSwapChainImageViews.resize(mSwapChainImages.size(), vk::Deleter<VkImageView>{ mDevice, vkDestroyImageView });

		// now, iterate over all of the swap chain images
		for (uint32_t i = 0; i < mSwapChainImages.size(); ++i)
		{
			createImageView(mSwapChainImages[i], mSwapChainImageFormat, mSwapChainImageViews[i]);
		}

		std::cout << "Successfully created " << mSwapChainImageViews.size() << " image views." << std::endl;
	}

	//! create a descriptor set layout to describe the types of resources that are going to be accessed by the graphics pipeline
	void createDescriptorSetLayout()
	{
		// uniform buffer object
		VkDescriptorSetLayoutBinding uboLayoutBinding = {};
		uboLayoutBinding.binding = 0;
		uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboLayoutBinding.descriptorCount = 1;
		uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;	// the shader stage(s) that will reference this descriptor (could use VK_SHADER_STAGE_ALL_GRAPHICS)
		uboLayoutBinding.pImmutableSamplers = nullptr;				// optional

		// combined image sampler object
		VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
		samplerLayoutBinding.binding = 1;
		samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		samplerLayoutBinding.pImmutableSamplers = nullptr;

		// both bindings
		std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, samplerLayoutBinding };

		VkDescriptorSetLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = bindings.size();
		layoutInfo.pBindings = bindings.data();

		if (vkCreateDescriptorSetLayout(mDevice, &layoutInfo, nullptr, &mDescriptorSetLayout) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create descriptor set layout object.");
		}
	}

	//! create a descriptor pool, from which we can allocate descriptor sets
	void createDescriptorPool()
	{
		/*

		Descriptor sets can't be created directly, they must be allocated from a pool like command
		buffers. A descriptor set specifies a VkBuffer resource to bind to the uniform buffer
		descriptor.

		*/

		std::array<VkDescriptorPoolSize, 2> poolSizes = {};
		poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;			// ubo
		poolSizes[0].descriptorCount = 1;
		poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;	// sampler
		poolSizes[1].descriptorCount = 1;

		VkDescriptorPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = poolSizes.size();
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = 1;

		if (vkCreateDescriptorPool(mDevice, &poolInfo, nullptr, &mDescriptorPool) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create descriptor pool.");
		}

		std::cout << "Successfully created descriptor pool object." << std::endl;
	}

	//! create a descriptor set 
	void createDescriptorSet()
	{
		/*

		Shaders access buffer and image resources by using special shader variables which are indirectly
		bound to buffer and image views via the API. These variables are organized into sets, where each
		set of bindings is represented by a descriptor set object in the API and a descriptor set is
		bound all at once.

		A descriptor is an opaque data structure representing a shader resource such as a buffer view,
		image view, sampler, or combined image sampler. The content of each is determined by its descriptor
		set layout and the sequence of set layouts that can be used by resource variables in shaders
		within a pipeline is specified in a pipeline layout.

		*/

		VkDescriptorSetLayout layouts[] = { mDescriptorSetLayout };
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = mDescriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = layouts;

		if (vkAllocateDescriptorSets(mDevice, &allocInfo, &mDescriptorSet) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to allocated descriptor set.");
		}

		std::cout << "Successfully allocated descriptor set." << std::endl;

		// configure the descriptor for the ubo
		VkDescriptorBufferInfo bufferInfo = {};
		bufferInfo.buffer = mUniformBuffer;
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UniformBufferObject);

		// configure the descriptor for the sampler
		VkDescriptorImageInfo imageInfo = {};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = mTextureImageView;
		imageInfo.sampler = mTextureSampler;

		std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};

		// ubo
		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = mDescriptorSet;
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &bufferInfo;
		descriptorWrites[0].pImageInfo = nullptr;		// optional
		descriptorWrites[0].pTexelBufferView = nullptr;	// optional

		// sampler
		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].dstSet = mDescriptorSet;
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pBufferInfo = nullptr;
		descriptorWrites[1].pImageInfo = &imageInfo;	// optional
		descriptorWrites[1].pTexelBufferView = nullptr;	// optional

		vkUpdateDescriptorSets(mDevice, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
	}

	//! sets up a rendering pipeline by creating shader modules and specifying viewport, scissor, blend, rasterizer, and multisampling settings
	void createGraphicsPipeline()
	{
		/*
		
		Note that the shader module objects are only required during the pipeline creation process. So, 
		instead of declaring them as class members, we'll make them local variables in this function.
		
		The VkShaderModule object is just a dumb wrapper around the bytecode buffer. To link the shaders
		together, we need to create a VkPipelineShaderStageCreateInfo structure, which is part of the 
		actual pipeline creation process.

		The VkPipelineVertexInputStateCreateInfo structure describes the format of the vertex data that 
		will be passed to the vertex shader in two ways:

		1. Bindings: spacing between data and whether the data is per-vertex or per-instance
		2. Attribute descriptions: type of the attributes pass to the vertex shader, which binding to load
		   them from and at which offset

		The VkPipelineInputAssemblyStateCreateInfo structure describes two things: what kind of geometry will
		be drawn from the vertices and if primitive restart should be enabled. The former is specified in the
		topology member and can have values like VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP.

		The rasterizer takes the vertices from the vertex shader and turns them into fragments to be colored
		by the fragment shader. It also performs depth testing, face culling, and the scissor test, and it 
		can be configured to output fragments that fill entire polygons or just the edges (wireframe). All of
		that is configured using the VkPipelineRasterizationStateCreateInfo structure.

		The VkPipelineMultisampleStateCreateInfo structure configures multisampling, which is one of the ways
		to perform anti-aliasing. It works by combining the fragment shader results of multiple polygons that
		rasterize to the same pixel. This mainly occurs along edges. Because it doesn't need to run the fragment
		shader multiple times if only one polygon maps to a pixel, it is significantly less expensive than simply
		rendering to a higher resolution and then downscaling.

		There are two types of structures to configure color blending. The first, VkPipelineColorBlendAttachmentState,
		contains the configuration per attached framebufer. The second, VkPipelineColorBlendStateCreateInfo, 
		contains the global color blending settings. In our case, we only have one framebuffer.

		You can use uniform values in shaders, but they need to be specified during pipeline creation by creating a
		VkPipelineLayout object. Even though we won't be using one for now, we will still create an empty object
		for demonstration purposes.

		*/

		auto vertShaderCode = readFile("shaders/vert.spv");
		auto fragShaderCode = readFile("shaders/frag.spv");

		vk::Deleter<VkShaderModule> vertShaderModule{ mDevice, vkDestroyShaderModule };
		vk::Deleter<VkShaderModule> fragShaderModule{ mDevice, vkDestroyShaderModule };

		createShaderModule(vertShaderCode, vertShaderModule);
		createShaderModule(fragShaderCode, fragShaderModule);

		VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main"; // the function to invoke (main is standard)

		VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main"; // the function to invoke (main is standard)

		// an array of the two pipeline structs
		VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

		// describe the vertex data
		auto bindingDescription = Vertex::getBindingDescription();
		auto attributeDescriptions = Vertex::getAttributeDescriptions();
		VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.vertexAttributeDescriptionCount = attributeDescriptions.size();
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

		// describe what type of geometry will be drawn
		VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		// specify the viewport
		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)mSwapChainExtent.width;
		viewport.height = (float)mSwapChainExtent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		// draw the entire framebuffer, so specify a scissor rectangle that covers it entirely
		VkRect2D scissor = {};
		scissor.offset = { 0, 0 };
		scissor.extent = mSwapChainExtent;

		// now, combine the viewport and scissor rectangles into a viewport state
		VkPipelineViewportStateCreateInfo viewportState = {};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;

		// configure the rasterizer
		VkPipelineRasterizationStateCreateInfo rasterizer = {};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE; // fragments beyond the near and far planes are clamped as opposed to discarded if this is true
		rasterizer.rasterizerDiscardEnable = VK_FALSE; // disables output to any framebuffer if this is true
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL; // how fragments are generated for geometry
		rasterizer.lineWidth = 1.0f; // the thickness of lines in terms of number of fragments
		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT; // the type of face culling to use
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // the vertex order for faces to be considered front-facing
		rasterizer.depthBiasClamp;
		rasterizer.depthBiasConstantFactor = 0.0f; // optional
		rasterizer.depthBiasClamp = 0.0f; // optional
		rasterizer.depthBiasSlopeFactor = 0.0f; // optional

		// for now, we will not use multisampling
		VkPipelineMultisampleStateCreateInfo multisampling = {};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampling.minSampleShading = 1.0f; // optional
		multisampling.pSampleMask = nullptr; // optional
		multisampling.alphaToCoverageEnable = VK_FALSE; // optional
		multisampling.alphaToOneEnable = VK_FALSE; // optional

		// normally, we might have to configure depth and/or stencil buffers here, but we will set them to nullptrs for now...

		// setup color blending
		VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE; // since this is false, the new color from the fragment shader is passed through unmodified
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // optional
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // optional
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // optional
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // optional
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // optional
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // optional

		/*
		
		Alpha blending could be enabled like so:

		colorBlendAttachment.blendEnable = VK_TRUE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

		*/

		// set blend constants that you can use as blend factors in the aforementioned calculations
		VkPipelineColorBlendStateCreateInfo colorBlending = {};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY; // optional
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;
		colorBlending.blendConstants[0] = 0.0f; // optional
		colorBlending.blendConstants[1] = 0.0f; // optional
		colorBlending.blendConstants[2] = 0.0f; // optional
		colorBlending.blendConstants[3] = 0.0f; // optional

		// setup pipeline layout, which controls access to descriptor sets and push constants
		VkDescriptorSetLayout setLayouts[] = { mDescriptorSetLayout };
		VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = setLayouts;
		pipelineLayoutInfo.pushConstantRangeCount = 0;
		pipelineLayoutInfo.pPushConstantRanges = 0;

		if (vkCreatePipelineLayout(mDevice, &pipelineLayoutInfo, nullptr, &mPipelineLayout) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to created pipeline layout.");
		}

		// create the graphics pipeline 
		VkGraphicsPipelineCreateInfo pipelineInfo = {};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStages;
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = nullptr; // optional
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = nullptr; // optional

		pipelineInfo.layout = mPipelineLayout; // handle (rather than struct pointer)

		pipelineInfo.renderPass = mRenderPass; // handle to render pass (created prior to this function call)
		pipelineInfo.subpass = 0;

		// Vulkan allows you to create a new graphics pipeline by deriving it from an existing pipeline: null for now
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.basePipelineIndex = -1;

		if (vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mGraphicsPipeline) != VK_SUCCESS) 
		{
			throw std::runtime_error("Failed to create graphics pipeline.");
		}
		
		std::cout << "Successfully created graphics pipeline object." << std::endl;
	}

	//! create a render pass object for use in the graphics pipeline
	void createRenderPass()
	{
		/*
		
		Before we can finish creating the graphics pipeline, we need to tell Vulkan about the framebuffer
		attachments that will be used for rendering. We need to specify how many color and depth buffers 
		there will be, how many samples to use for each of them, and how their contents should be handled
		throughout the rendering process. All of this information is wrapped into a render pass object.

		For now, we'll just have a single color buffer attachment represented by one of the images from the
		swap chain. It's format should match the one of the swap chain images, and we're not doing any
		multisampling, so we stick to 1 sample.

		The loadOp and storeOp determine what to do with the data in the attachment before rendering and after 
		rendering. VK_ATTACHMENT_LOAD_OP_CLEAR implies that we will clear the values to a constant at the 
		start. VK_ATTACHMENT_STORE_OP_STORE implies that the rendered contents will be stored in memory and
		can be read later. We are using a stencil buffer, so those values don't matter at the moment.
		
		Textures and framebuffers in Vulkan are represented by VkImage objects with a certain pixel format,
		however the layout of the pixels in memory can change based on what you're trying to do with an image.
		Some of the most common layouts are:

		1. VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: images used as color attachments
		2. VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: images to be presented in the swap chain
		3. VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: images to be used as the destination for a memory copy operation

		What's important right now is that images need to be transitioned to specific layouts that are 
		suitable for the operation that they're going to be involved in next.

		The initialLayout member specifies which layout the image will have before the render pass begins. 
		The finalLayout member specifies which layout to automatically transition to when the render pass
		finishes. Using VK_IMAGE_LAYOUT_UNDEFINED for initialLayout means that we don't care what previous
		layout the image was in. The caveat of this special value is that the contents of the image are not
		guaranteed to be preserved, but that doesn't matter since we're going to clear it anyway. We want
		the image to be ready for presentation using the swap chain after rendering, which is why we use
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR as finalLayout.

		A single render pass can consist of multiple subpasses. These are subsequent rendering operations
		that depend on the contents of framebuffers in previous passes, for example a sequence of post-processing
		effects that are applied one after another. For now, we stick to one subpass.

		*/
		
		VkAttachmentDescription colorAttachment = {};
		colorAttachment.format = mSwapChainImageFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; 
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		// every subpass references one or more of the attachments that we've described using the structure in the previous sections
		// the index of the attachment in this array is directly referenced from the fragment shader with the layout directive
		VkAttachmentReference colorAttachmentRef = {};
		colorAttachmentRef.attachment = 0; // our array consists of a single VkAttachmentDescription so the index is 0
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// describe the subpass
		VkSubpassDescription subPass = {};
		subPass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // Vulkan may support compute subpasses in the future...
		subPass.colorAttachmentCount = 1;
		subPass.pColorAttachments = &colorAttachmentRef;

		// describe subpass dependencies
		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL; // the implicit subpass before or after the render pass depending on whether it is specified in srcSubpass or dstSubpass
		dependency.dstSubpass = 0; // our subpass, which is the first and only one
		dependency.srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependency.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT; // we need to wait for the swap chain to finish reading from the image before we can access it
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		// create the render pass object
		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subPass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		if (vkCreateRenderPass(mDevice, &renderPassInfo, nullptr, &mRenderPass) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create render pass.");
		}
		
		std::cout << "Successfully created the render pass object." << std::endl;
	}

	//! helper function for loading SPIR-V binary data
	static std::vector<char> readFile(const std::string& filename)
	{
		// ate: start reading the file at the beginning so that we can use the read position to determine the size of the file for buffer allocation
		// binary: read the file as a binary file (avoid text transformations)
		std::ifstream file(filename, std::ios::ate | std::ios::binary);
		if (!file.is_open())
		{
			throw std::runtime_error("Failed to open file " + filename);
		}

		// determine file size
		size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer(fileSize);

		// seek back to the beginning and read all of the bytes at once
		file.seekg(0);
		file.read(buffer.data(), fileSize);

		// close the file and return the bytes
		file.close();

		std::stringstream ss;
		ss << fileSize;
		
		std::cout << "Successfully loaded file " << filename << " with " << ss.str() << " bytes." << std::endl;

		return buffer;
	}

	//! create a shader module
	void createShaderModule(const std::vector<char>& code, vk::Deleter<VkShaderModule>& shaderModule)
	{
		/*
		
		Instead of returning a shader module handle directly, it will be written to the variable specified
		for the second parameter.

		*/

		VkShaderModuleCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = (uint32_t*)code.data();

		if (vkCreateShaderModule(mDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create shader module.");
		}
	}

	//! create framebuffer objects
	void createFramebuffers()
	{
		/*
		
		The attachments specified during render pass creation are bound by wrapping them into a VkFramebuffer
		object. A framebuffer object references all of the VkImageView objects that represent the attachments.
		In our case, that will only be a single attachment: the color attachment. However, the image that we have
		to use as an attachment depends on which image the swap chain returns when we retrieve one for 
		presentation. That means we have to create a framebuffer for all of the images in the swap chain and use 
		the one that corresponds to the retrieved image at drawing time.
		
		*/
		
		mSwapChainFramebuffers.resize(mSwapChainImageViews.size(), vk::Deleter<VkFramebuffer>{ mDevice, vkDestroyFramebuffer });

		// iterate through the image views and create framebuffers from them
		for (size_t i = 0; i < mSwapChainImageViews.size(); ++i)
		{
			VkImageView attachments[] = { mSwapChainImageViews[i] };

			VkFramebufferCreateInfo framebufferInfo = {};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = mRenderPass;
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments = attachments;
			framebufferInfo.width = mSwapChainExtent.width;
			framebufferInfo.height = mSwapChainExtent.height;
			framebufferInfo.layers = 1;

			if (vkCreateFramebuffer(mDevice, &framebufferInfo, nullptr, &mSwapChainFramebuffers[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to create framebuffer.");
			}
		}

		std::cout << "Successfully created all framebuffer objects." << std::endl;
	}

	//! create a command pool, which manages and allocates command buffers
	void createCommandPool()
	{
		/*
		
		Commands in Vulkan, like drawing operations and memory transfers, are not executed directly using
		function calls. You have to record all of the operations you want to perform in command buffer
		objects. The advantage of this is that all of the hard work of setting up the drawing commands can 
		be done in advance and in multiple threads. After that, you just have to tell Vulkan to execute the
		commands in the main loop.

		We have to create a command pool before we can create command buffers. Command pools manage the 
		memory that is used to store the buffers and command buffers are allocated from them.
		
		Command buffers are executed by submitting them on one of the device queues, like the graphics and
		presentation queues we retrieved. Each command pool can only allocate command buffers that are 
		submitted on a single type of queue. We're going to record commands for drawing, which is why
		we've chosen the graphics queue family.

		There are two possible flags for command pools:

		1. VK_COMMAND_POOL_CREATE_TRANSIENT_BIT: command buffers are re-recorded with new commands very often
		2. VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT: allow command buffers to be re-recorded individually,
		   without this flag they all have to be reset together

		*/

		QueueFamilyIndices queueFamilyIndices = findQueueFamilies(mPhysicalDevice); 

		VkCommandPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;
		poolInfo.flags = 0; // optional

		if (vkCreateCommandPool(mDevice, &poolInfo, nullptr, &mCommandPool) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create command pool.");
		}

		std::cout << "Successfully created command pool object." << std::endl;
	}

	//! create an image from a STB image
	void createTextureImage()
	{
		int texWidth, texHeight, texChannels;
		stbi_uc* pixels = stbi_load("textures/texture.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		std::cout << "Loaded STB image file, resolution: " << texWidth << " x " << texHeight << std::endl;

		VkDeviceSize imageSize = texWidth * texHeight * 4;
		
		if (!pixels)
		{
			throw std::runtime_error("Failed to load STB image file.");
		}

		vk::Deleter<VkImage> stagingImage{ mDevice, vkDestroyImage };
		vk::Deleter<VkDeviceMemory> stagingImageMemory{ mDevice, vkFreeMemory };

		// create staging image and memory
		createImage(texWidth, 
			texHeight, 
			VK_FORMAT_R8G8B8A8_UNORM, 
			VK_IMAGE_TILING_LINEAR, 
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
			stagingImage, 
			stagingImageMemory);

		// transfer pixel data from CPU to GPU (host visible) memory
		void* data;
		vkMapMemory(mDevice, stagingImageMemory, 0, imageSize, 0, &data);
		memcpy(data, pixels, (size_t)imageSize);
		vkUnmapMemory(mDevice, stagingImageMemory);

		std::cout << "Successfully loaded STB image data into host visible (staging) memory." << std::endl;

		// create final image and memory 
		createImage(texWidth, 
			texHeight,
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, // we want to be able to sample texels from it in the shader
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			mTextureImage,
			mTextureImageMemory);

		// prepare the images for a copy operation by transitioning their layouts 
		transitionImageLayout(stagingImage, VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		transitionImageLayout(mTextureImage, VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		copyImage(stagingImage, mTextureImage, texWidth, texHeight);

		// to enable sampling from the newly created texture image, we need to do one more layout transition
		transitionImageLayout(mTextureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		// free the CPU-side memory
		stbi_image_free(pixels);
	}
	
	//! a helper function for creating an image and its associated memory
	void createImage(uint32_t width,
		uint32_t height,
		VkFormat format,
		VkImageTiling tiling,
		VkImageUsageFlags usage,
		VkMemoryPropertyFlags properties,
		vk::Deleter<VkImage>& image,
		vk::Deleter<VkDeviceMemory>& imageMemory)
	{
		VkImageCreateInfo imageInfo = {};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = width;
		imageInfo.extent.height = height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = format;									// format that the loaded pixels will be converted to
		imageInfo.tiling = tiling;									// either VK_IMAGE_TILING_LINEAR (row-major order) or VK_IMAGE_TILING_OPTIMAL (implementation defined)
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;	// not usable by the GPU, but the first transition will preserve the texels
		imageInfo.usage = usage;									// set up as transfer source (since this is the staging image)
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;			// only used by one queue family: the one that supports transfer operations
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;					// related to multisampling (only relevant for images that will be used as attachments)
		imageInfo.flags = 0;										// optional

		if (vkCreateImage(mDevice, &imageInfo, nullptr, &image) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create image.");
		}

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(mDevice, image, &memRequirements);

		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

		if (vkAllocateMemory(mDevice, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to allocation image memory.");
		}

		vkBindImageMemory(mDevice, image, imageMemory, 0);
	}

	//! a helper function for transitioning between two image layouts
	void transitionImageLayout(VkImage image,
		VkImageLayout oldLayout,
		VkImageLayout newLayout)
	{
		VkCommandBuffer commandBuffer = beginSingleTimeCommands();

		// transitioning image layouts requires synchronization: for this, we use a type of pipeline barrier
		VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // we aren't using the barrier to transfer queue family ownership, but this field is still required
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		
		// we need to set our srcAccessMask and dstAccessMask based on the transitions we intend to handle
		// 1. preinitialized -> transfer source: transfer reads should wait on host writes
		// 2. preinitialized -> transfer destination: transfer writes should wait on host writes
		// 3. transfer destination -> shader reading: shader reads should wait on transfer writes
		if (oldLayout == VK_IMAGE_LAYOUT_PREINITIALIZED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) 
		{
			barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_PREINITIALIZED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) 
		{
			barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) 
		{
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		}
		else 
		{
			throw std::invalid_argument("Unsupported image layout transition.");
		}

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // the pipeline stage in which operations will wait on the barrier: we want it to happen immediately
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

		endSingleTimeCommands(commandBuffer);
	}

	//! a helper function for copying data between two images
	void copyImage(VkImage srcImage,
		VkImage dstImage,
		uint32_t width,
		uint32_t height)
	{
		VkCommandBuffer commandBuffer = beginSingleTimeCommands();

		VkImageSubresourceLayers subResource = {};
		subResource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subResource.baseArrayLayer = 0;
		subResource.mipLevel = 0;
		subResource.layerCount = 1;

		VkImageCopy region = {};
		region.srcSubresource = subResource;
		region.dstSubresource = subResource;
		region.srcOffset = { 0, 0, 0 };
		region.dstOffset = { 0, 0, 0 };
		region.extent.width = width;
		region.extent.height = height;
		region.extent.depth = 1;

		vkCmdCopyImage(commandBuffer,
			srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &region);

		endSingleTimeCommands(commandBuffer);
	}

	//! create an image view that grants access to the texture image
	void createTextureImageView()
	{
		createImageView(mTextureImage, VK_FORMAT_R8G8B8A8_UNORM, mTextureImageView);
		std::cout << "Successfully created texture image view for STB image." << std::endl;
	}

	//! a helper function for creating an image view from an image
	void createImageView(VkImage image,
		VkFormat format,
		vk::Deleter<VkImageView>& imageView)
	{
		VkImageViewCreateInfo viewInfo = {};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = format;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(mDevice, &viewInfo, nullptr, &imageView) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create texture image view.");
		}
	}

	//! create a sampler to extract colors from an image
	void createTextureSampler()
	{
		// note that the sampler does not reference a VkImage anywhere: it is an independent object that provides 
		// an interface to extract colors from a texture and can be applied to any image you want (1D, 2D, or 3D)
		VkSamplerCreateInfo samplerInfo = {};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.anisotropyEnable = VK_TRUE;
		samplerInfo.maxAnisotropy = 16;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE; // we want to use texture coordinates from 0...1
		samplerInfo.compareEnable = VK_FALSE;			// mostly used for percentage-close filtering
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 0.0f;
		
		if (vkCreateSampler(mDevice, &samplerInfo, nullptr, &mTextureSampler) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create texture sampler.");
		}

		std::cout << "Successfully created texture sampler object." << std::endl;
	}

	//! a helper function for abstracting buffer creation
	void createBuffer(VkDeviceSize size, 
		VkBufferUsageFlags usage, 
		VkMemoryPropertyFlags properties,
		vk::Deleter<VkBuffer>& buffer, 
		vk::Deleter<VkDeviceMemory>& bufferMemory)
	{
		/*

		Just like the images in the swap chain, buffers can also be owned by a specific queue family or
		be shared between multiple at the same time. We will only be using this buffer from the graphics
		queue, so we can stick to exclusive access.

		The flags parameter is used to configure sparse buffer memory, which is not relevant right now.

		The VkMemoryRequirements struct that is returned by vkGetBufferMemoryRequirements has three fields:

		1. size: the size of the required amount of memory, in bytes, which may differ from bufferInfo.size
		2. alignment: the offset, in bytes, where the buffer begins in the allocated region of memory (depends
		on bufferInfo.usage and bufferInfo.flags)
		3. memoryTypeBits: bit field of the memory types that are suitable for the buffer

		*/

		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateBuffer(mDevice, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create buffer object.");
		}

		// query memory requirements
		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(mDevice, buffer, &memRequirements);

		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
		
		if (vkAllocateMemory(mDevice, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to allocate buffer memory.");
		}

		std::cout << "Successfully allocated " << memRequirements.size << " bytes of buffer memory." << std::endl;

		// associate this memory with the buffer
		vkBindBufferMemory(mDevice, buffer, bufferMemory, 0); // if the offset is non-zero, then it is required to be divisible by memRequirements.alignment
	}

	//! create a GPU-side buffer to hold the specified vertex data
	void createVertexBuffer()
	{
		/*
		
		The memory type that allows us to access it from the CPU may not be the most optimal memory
		type for the graphics card itself to read from. The most optimal memory has the VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		flag and is usually not accessible by the CPU on dedicated graphics cards. The buffer copy
		command requires a queue family that supports transfer operations, which is indicated using the
		VK_QUEUE_TRANSFER_BIT. Any queue family with VK_QUEUE_GRAPHICS_BIT or VK_QUEUE_COMPUTE_BIT capabilities
		already implicitly supports transfer operations.

		*/
		
		VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

		// create a staging buffer
		vk::Deleter<VkBuffer> stagingBuffer{ mDevice, vkDestroyBuffer };
		vk::Deleter<VkDeviceMemory> stagingBufferMemory{ mDevice, vkFreeMemory };
		createBuffer(bufferSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT, // buffer can be used as the source in a memory transfer operation
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer,
			stagingBufferMemory);

		// copy CPU-side vertex data into the staging buffer
		void* data;
		vkMapMemory(mDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, vertices.data(), (size_t)bufferSize);
		vkUnmapMemory(mDevice, stagingBufferMemory);

		// create a vertex buffer
		createBuffer(bufferSize,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, // buffer can be used as the destination in a memory transfer operation
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			mVertexBuffer,
			mVertexBufferMemory);

		// copy data between buffers
		copyBuffer(stagingBuffer, mVertexBuffer, bufferSize);
	}
	
	//! create a GPU-side buffer to hold the specified vertex indices
	void createIndexBuffer()
	{
		VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

		// create a staging buffer
		vk::Deleter<VkBuffer> stagingBuffer{ mDevice, vkDestroyBuffer };
		vk::Deleter<VkDeviceMemory> stagingBufferMemory{ mDevice, vkFreeMemory };
		createBuffer(bufferSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer,
			stagingBufferMemory);

		// copy CPU-side vertex data into the staging buffer
		void* data;
		vkMapMemory(mDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, indices.data(), (size_t)bufferSize);
		vkUnmapMemory(mDevice, stagingBufferMemory);

		// create a vertex buffer
		createBuffer(bufferSize,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			mIndexBuffer,
			mIndexBufferMemory);

		// copy data between buffers
		copyBuffer(stagingBuffer, mIndexBuffer, bufferSize);
	}

	//! create a GPU-side buffer to hold shader uniforms
	void createUniformBuffer()
	{
		VkDeviceSize bufferSize = sizeof(UniformBufferObject);

		createBuffer(bufferSize, 
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
			mUniformStagingBuffer, 
			mUniformStagingBufferMemory);

		createBuffer(bufferSize, 
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
			mUniformBuffer, 
			mUniformBufferMemory);
	}

	//! creates a temporary command buffer for copying data between two buffers
	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
	{
		VkCommandBuffer commandBuffer = beginSingleTimeCommands();

		// copy command
		VkBufferCopy copyRegion = {};
		copyRegion.srcOffset = 0; // optional
		copyRegion.dstOffset = 0; // optional
		copyRegion.size = size;
		vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

		endSingleTimeCommands(commandBuffer);
	}

	//! record into a single-use command buffer
	VkCommandBuffer beginSingleTimeCommands()
	{
		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = mCommandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(mDevice, &allocInfo, &commandBuffer);

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;	// only use the command buffer once
							   
		vkBeginCommandBuffer(commandBuffer, &beginInfo);				// begin recording into the command buffer

		return commandBuffer;
	}

	//! finish recording into a single-use command buffer
	void endSingleTimeCommands(VkCommandBuffer commandBuffer)
	{
		vkEndCommandBuffer(commandBuffer);

		// submit the command buffer to the queue
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(mGraphicsQueue); // wait for the transfer operation to finish

		vkFreeCommandBuffers(mDevice, mCommandPool, 1, &commandBuffer);
	}

	//! called from createVertexBuffer to find the appropriate memory type to use 
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
	{
		/*
		
		Graphics cards can offer different types of memory to allocate from. Each type of memory varies in 
		terms of the operations it allows and performance. We need to combine the requirements of the vertex buffer
		and our own application to find the right type of memory to use.

		The VkPhysicalDeviceMemoryProperties struct has two arrays, memoryTypes and memoryHeaps. Memory heaps 
		are distinct memory resources like dedicated VRAM and swap space in RAM for when VRAM runs out. The
		different types of memory exist within these heaps. Right now, we'll only concern ourselves with the
		type of memory and not the heap it comes from.

		*/

		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &memProperties);
		
		for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
		{
			// see if the type we are looking for is "on" (i.e. that bit is set to 1)
			// we also need to be able to write our vertex data to this memory - the memoryTypes array consists of
			//     VkMemoryHeap structs that specify the heap and properties of each type of memory (the properties
			//	   define special features of the memory, like being able to map it from the CPU for writing)
			if (typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) 
			{
				return i;
			}
		}

		throw std::runtime_error("Failed to find a suitable memory type.");
	}

	//! create command buffers, which record drawing or compute commands
	void createCommandBuffers()
	{
		/*
		
		Because one of the drawing commands involves binding the right VkFramebuffer, we actually need
		to record a command buffer for every image in the swap chain once again. 

		We begin recording a command buffer by calling vkBeginCommandBuffer with a small VkCommandBufferBeginInfo
		structure as an argument that specifies some details about the usage of this specific command 
		buffer. The flags member of this structure specifies how we're going to use the command buffer. The
		following values are available:
		
		1. VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT: the buffer will be re-recorded right after executing it once
		2. VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT: this is a secondary buffer that will be entirely
		   within a single render pass
		3. VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT: the buffer can be re-submitted while it is also 
		   already pending execution

		We use the last flag because we may already be scheduling the drawing commands for the next frame while
		the last frame is not finished yet. The pInheritanceInfo parameter is only relevant for secondary command
		buffers. It specifies which state to inherit from the calling primary command buffers.

		Drawing starts by beginning the render pass with vkCmdBeginRenderPass. The render pass is configured
		using some parameters in a VkRenderPassBeginInfo structure.

		Note how all commands in Vulkan begin with this vkCmd* prefix. The first parameter is always the command
		buffer to record the command to. The second parameter specifies the details of the render pass we've just
		provided. The final parameter controls how the drawing commands within the render pass will be provided.
		It can have one of two values:

		1. VK_SUBPASS_CONTENTS_INLINE: the render pass commands will be embedded in the primary command buffer
		   itself and no secondary command buffers will be executed
		2. VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS: the render pass commands will be executed from secondary
		   command buffers

		*/

		// free any previous command buffers (this will happen via a call to recreateSwapChain)
		if (mCommandBuffers.size() > 0)
		{
			vkFreeCommandBuffers(mDevice, mCommandPool, mCommandBuffers.size(), mCommandBuffers.data());
			std::cout << "Freeing command buffers." << std::endl;
		}

		mCommandBuffers.resize(mSwapChainFramebuffers.size());

		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = mCommandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // whether the command buffers are primary or secondary command buffers
		allocInfo.commandBufferCount = (uint32_t)mCommandBuffers.size();

		if (vkAllocateCommandBuffers(mDevice, &allocInfo, mCommandBuffers.data()) != VK_SUCCESS) 
		{
			throw std::runtime_error("Failed to allocate command buffers.");
		}

		for (size_t i = 0; i < mCommandBuffers.size(); i++)
		{
			VkCommandBufferBeginInfo beginInfo = {};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
			beginInfo.pInheritanceInfo = nullptr; // optional

			// begin recording commands
			vkBeginCommandBuffer(mCommandBuffers[i], &beginInfo);

			VkRenderPassBeginInfo renderPassInfo = {};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.renderPass = mRenderPass;
			renderPassInfo.framebuffer = mSwapChainFramebuffers[i];
			renderPassInfo.renderArea.offset = { 0, 0 };			// the size of the render area
			renderPassInfo.renderArea.extent = mSwapChainExtent;

			VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };	// the clear values to use for VK_ATTACHMENT_LOAD_OP_CLEAR
			renderPassInfo.clearValueCount = 1;
			renderPassInfo.pClearValues = &clearColor;

			// begin the render pass
			vkCmdBeginRenderPass(mCommandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

			// bind the graphics pipeline: notice the second parameter which tells Vulkan that this is a graphics (not compute) pipeline
			vkCmdBindPipeline(mCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, mGraphicsPipeline);

			// bind the uniform buffer
			vkCmdBindDescriptorSets(mCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mDescriptorSet, 0, nullptr);

			// bind the vertex buffer
			VkBuffer vertexBuffers[] = { mVertexBuffer };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(mCommandBuffers[i], 0, 1, vertexBuffers, offsets);

			// bind the index buffer
			vkCmdBindIndexBuffer(mCommandBuffers[i], mIndexBuffer, 0, VK_INDEX_TYPE_UINT16);

			// actual draw command:
			// index count
			// instance count
			// first index
			// first instance
			vkCmdDrawIndexed(mCommandBuffers[i], indices.size(), 1, 0, 0, 0);

			// end the render pass
			vkCmdEndRenderPass(mCommandBuffers[i]);

			// finish recording into the command buffer
			if (vkEndCommandBuffer(mCommandBuffers[i]) != VK_SUCCESS) 
			{
				throw std::runtime_error("Failed to record command buffer.");
			}
		}
	}
	
	//! create semaphores, which are used to synchronize operations within or across command queues
	void createSemaphores()
	{
		VkSemaphoreCreateInfo semaphoreInfo = {};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		if (vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mImageAvailableSemaphore) != VK_SUCCESS ||
			vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mRenderFinishedSemaphore) != VK_SUCCESS) 
		{
			throw std::runtime_error("Failed to create semaphores.");
		}

		std::cout << "Successfully created semaphore objects." << std::endl;
	}

	//! render and present a frame
	void drawFrame()
	{
		/*
		
		This function will perform the following operations:

		1. Acquire an image from the swap chain
		2. Execute the command buffer with that image as an attachment in the framebuffer
		3. Return the image to the swap chain for presentation

		Each of these events is set in motion using a single function call, but they are executed asynchronously. 
		The function calls will return before the operations are actually finished and the order of execution is 
		also undefined. That is unfortunate, because each of the operations depends on the previous one finishing.

		There are two ways of synchronizing swap chain events: fences and semaphores. They're both objects that 
		can be used for coordinating operations by having one operation signal and another operation wait for a 
		fence or semaphore to go from the unsignaled to signaled state.

		The difference is that the state of fences can be accessed from your program using calls like vkWaitForFences 
		and semaphores cannot be. Fences are mainly designed to synchronize your application itself with rendering 
		operation, whereas semaphores are used to synchronize operations within or across command queues. We want to 
		synchronize the queue operations of draw commands and presentation, which makes semaphores the best fit.
		
		The first two parameters of vkAcquireNextImageKHR are the logical device and the swap chain from which
		we wish to acquire an image. The third parameter specifies a timeout in nanoseconds for an image to become
		available. Using the maximum value of a 64-bit unsigned integer disables the timeout. The next two parameters
		specify synchronization objects that are to be signaled when the presentation engine is finished using
		the image. That's the point in time where we can start drawing to it. It is possible to specify a semaphore,
		fence, or both. We're going to use our mImageAvailableSemaphore for that purpose.

		The last parameter specifies a variable to output the index of the swap chain image that has become 
		available. The index refers to the VkImage in our mSwapChainImages array. We're going to use that index
		to pick the right command buffer.
		
		Queue submission and synchronization is configured through parameters in the VkSubmitInfo structure. The
		first three parameters specify which semaphores to wait on before execution begins and in which stage(s)
		of the pipeline to wait. We want to wait with writing colors to the image until it's available, so we're 
		specifying the stage of the graphics pipeline that writes to the color attachment. That means that 
		theoretically the implementation can already start executing our vertex shader and such while the image 
		is not available yet. Each entry in the waitStages array corresponds to the semaphore with the same index
		in pWaitSemaphores.

		The next two parameters specify which command buffers to actually submit for execution. As mentioned earlier, 
		we should submit the command buffer that binds the swap chain image we just acquired as color attachment.

		The signalSemaphoreCount and pSignalSemaphores parameters specify which semaphores to signal once the 
		command buffer(s) have finished execution. In our case we're using the mRenderFinishedSemaphore for that purpose.

		We can now submit the command buffer to the graphics queue using vkQueueSubmit. The function takes an array of 
		VkSubmitInfo structures as argument for efficiency when the workload is much larger. The last parameter references 
		an optional fence that will be signaled when the command buffers finish execution. We're using semaphores for 
		synchronization, so we'll just pass a VK_NULL_HANDLE.

		The last step of drawing a frame is submitting the result back to the swap chain to have it eventually show 
		up on the screen. Presentation is configured through a VkPresentInfoKHR structure.

		*/
		
		// retrieve an image from the swap chain: it is possible for Vulkan to tell us that the swap chain is no longer compatible during presentation
		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(mDevice, mSwapChain, std::numeric_limits<uint64_t>::max(), mImageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			recreateSwapChain();
			return;
		}
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) // continue if we receive VK_SUBOPTIMAL_KHR...
		{
			throw std::runtime_error("Failed to acquire swap chain image.");
		}

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		// prepare the queue for submission
		VkSemaphore waitSemaphores[] = { mImageAvailableSemaphore };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &mCommandBuffers[imageIndex];

		VkSemaphore signalSemaphores[] = { mRenderFinishedSemaphore };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		if (vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) 
		{
			throw std::runtime_error("Failed to submit draw command buffer.");
		}

		// configure presentation 
		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapChains[] = { mSwapChain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.pResults = nullptr; // optional

		// similar to vkAcquireNextImageKHR, we check the result of presenting the newly rendered image and take action if necessary
		result = vkQueuePresentKHR(mPresentQueue, &presentInfo);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
		{
			recreateSwapChain();
		}
		else if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to present swap chain image.");
		}
	}

	//! rebuilds the entire swap chain 
	void recreateSwapChain()
	{
		/*
		
		If the window size changes, our swap chain will no longer be compatible with the window surface. We
		first call vkDeviceWaitIdle to ensure that we don't touch any resources that may still be in use.

		Obviously, the first thing we'll have to do is recreate the swap chain itself. The image views need 
		to be recreated because they are based directly on the swap chain images. The render pass needs to be 
		recreated because it depends on the format of the swap chain images. Viewport and scissor rectangle 
		size is specified during graphics pipeline creation, so the pipeline also needs to be rebuilt. It is
		possible to avoid this by using dynamic state for the viewports and scissor rectangles. Finally, the 
		framebuffers and command buffers also directly depend on the swap chain images.

		*/

		vkDeviceWaitIdle(mDevice);

		createSwapChain();
		createImageViews();
		createRenderPass();
		createGraphicsPipeline();
		createFramebuffers();
		createCommandBuffers();
	}

	/* General */
	GLFWwindow *mWindow;
	const int mWidth = 800;
	const int mHeight = 600;
	
	/* Instance, device, and surface related */
	vk::Deleter<VkInstance> mInstance{ vkDestroyInstance };
	vk::Deleter<VkSurfaceKHR> mSurface{ mInstance, vkDestroySurfaceKHR };
	VkPhysicalDevice mPhysicalDevice{ VK_NULL_HANDLE };									// implicitly destroyed when the VkInstance is destroyed, so we don't need to add a delete wrapper
	vk::Deleter<VkDevice> mDevice{ vkDestroyDevice };									// needs to be declared below the VkInstance, since it must be destroyed before the instance is cleaned up
	
	/* Queue related */
	VkQueue mGraphicsQueue;																// automatically created and destroyed alongside the logical device
	VkQueue mPresentQueue;
	
	/* Swap chain related */
	vk::Deleter<VkSwapchainKHR> mSwapChain{ mDevice, vkDestroySwapchainKHR };
	std::vector<VkImage> mSwapChainImages;												// automatically destroyed alongside the swap chain
	VkFormat mSwapChainImageFormat;
	VkExtent2D mSwapChainExtent;
	std::vector<vk::Deleter<VkImageView>> mSwapChainImageViews;							// unlike VkImages, VkImageView objects are created by us so we need to clean them up ourselves

	/* Graphics pipeline related */
	vk::Deleter<VkRenderPass> mRenderPass{ mDevice, vkDestroyRenderPass };
	vk::Deleter<VkDescriptorSetLayout> mDescriptorSetLayout{ mDevice, vkDestroyDescriptorSetLayout };
	vk::Deleter<VkDescriptorPool> mDescriptorPool{ mDevice, vkDestroyDescriptorPool };
	VkDescriptorSet mDescriptorSet;
	vk::Deleter<VkPipelineLayout> mPipelineLayout{ mDevice, vkDestroyPipelineLayout };	// for describing uniform layouts: should be destroyed before the render pass above
	vk::Deleter<VkPipeline> mGraphicsPipeline{ mDevice, vkDestroyPipeline };
	std::vector<vk::Deleter<VkFramebuffer>> mSwapChainFramebuffers;
	
	/* Buffers and device memory related */
	vk::Deleter<VkBuffer> mVertexBuffer{ mDevice, vkDestroyBuffer };
	vk::Deleter<VkDeviceMemory> mVertexBufferMemory{ mDevice, vkFreeMemory };
	vk::Deleter<VkBuffer> mIndexBuffer{ mDevice, vkDestroyBuffer };
	vk::Deleter<VkDeviceMemory> mIndexBufferMemory{ mDevice, vkFreeMemory };
	vk::Deleter<VkBuffer> mUniformStagingBuffer{ mDevice, vkDestroyBuffer };
	vk::Deleter<VkDeviceMemory> mUniformStagingBufferMemory{ mDevice, vkFreeMemory };
	vk::Deleter<VkBuffer> mUniformBuffer{ mDevice, vkDestroyBuffer };
	vk::Deleter<VkDeviceMemory> mUniformBufferMemory{ mDevice, vkFreeMemory };
	
	/* Textures and samplers related */
	vk::Deleter<VkImage> mTextureImage{ mDevice, vkDestroyImage };
	vk::Deleter<VkDeviceMemory> mTextureImageMemory{ mDevice, vkFreeMemory };
	vk::Deleter<VkImageView> mTextureImageView{ mDevice, vkDestroyImageView };
	vk::Deleter<VkSampler> mTextureSampler{ mDevice, vkDestroySampler };

	/* Command pool related */
	vk::Deleter<VkCommandPool> mCommandPool{ mDevice, vkDestroyCommandPool };
	std::vector<VkCommandBuffer> mCommandBuffers;										// automatically freed when the VkCommandPool is destroyed

	/* Semaphore related */
	vk::Deleter<VkSemaphore> mImageAvailableSemaphore{ mDevice, vkDestroySemaphore };
	vk::Deleter<VkSemaphore> mRenderFinishedSemaphore{ mDevice, vkDestroySemaphore };

	/* Validation layer and extension related */
	const std::vector<const char*> mDeviceExtensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	vk::Deleter<VkDebugReportCallbackEXT> mCallback{ mInstance, DestroyDebugReportCallbackEXT };

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

	const std::vector<const char*> mValidationLayers{ "VK_LAYER_LUNARG_standard_validation" }; 
	
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
}	