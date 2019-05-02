#include <ntlab_CLCommandLineAppBase.h>

#define VECTOR_ADD_AS_SINGLE_WORK_ITEM 0

/** Inherits from CLCommandLineAppBase to hide away a lot of uninteresting boilerplate code */
class CLVectorAddDemo : public ntlab::CLCommandLineAppBase
{
public:
    CLVectorAddDemo (const std::string& platformToUse, const std::string& deviceToUse)

    : ntlab::CLCommandLineAppBase (platformToUse,
                                   deviceToUse,
#ifdef OPEN_CL_INTEL_FPGA
                                   ntlab::OpenCLHelpers::getAbsolutePathFromPathRelativeToExecutable("vectorAdd.aocx"),
                                   true)
#else
                                   ntlab::OpenCLHelpers::getAbsolutePathFromPathRelativeToExecutable("vectorAdd.cl"),
                                   false)
#endif
    {}
    
private:
    int runCLApplication() override
    {
        // Create some host side data buffers
        std::vector<int> sourceA (vectorSize);
        std::vector<int> sourceB (vectorSize);
        std::vector<int> dest    (vectorSize);
        
        // Create some CL buffer objects refering to the buffers on the device
        cl::Buffer bufferA (context, CL_MEM_READ_ONLY,  sizeof (int) * vectorSize);
        cl::Buffer bufferB (context, CL_MEM_READ_ONLY,  sizeof (int) * vectorSize);
        cl::Buffer bufferC (context, CL_MEM_READ_WRITE, sizeof (int) * vectorSize);
        
        // Fill the source buffers with some data
        for (int i = 0; i < vectorSize; ++i)
        {
            sourceA[i] = static_cast<int> (vectorSize - i);
            sourceB[i] = i * i;
        }
        
        // Create two event objects that let us monitor the state of both buffer writing tasks performed next
        std::vector<cl::Event> bufferEvents (2);
        
        // Enqueuing the write buffer tasks means to add the job of putting the host side data into the device side
        // buffers. The CL_FALSE argument makes this an async operation
        queue.enqueueWriteBuffer (bufferA, CL_FALSE, 0, sizeof (int) * sourceA.size(), sourceA.data(), nullptr, &bufferEvents[0]);
        queue.enqueueWriteBuffer (bufferB, CL_FALSE, 0, sizeof (int) * sourceB.size(), sourceB.data(), nullptr, &bufferEvents[1]);
        
        // As the enqueuing was set to be asynchronous above, this callback will be executed as soon as the enqueuing job has been completed in the background
        bufferEvents[0].setCallback (CL_COMPLETE, [] (cl_event, cl_int, void*) { std::cout << "Enqueueing buffer A completed\n";});
        bufferEvents[1].setCallback (CL_COMPLETE, [] (cl_event, cl_int, void*) { std::cout << "Enqueueing buffer B completed\n";});
        
        // Create a kernel object which represents the cl kernel function "vectorAdd" or "vectorAddSingleWorkItem" which
        // can be found in the program
#if VECTOR_ADD_AS_SINGLE_WORK_ITEM
        cl::Kernel kernelAdd (program, "vectorAddSingleWorkItem");
        kernelAdd.setArg (3, static_cast<int> (vectorSize));
#else
        cl::Kernel kernelAdd (program, "vectorAdd");
#endif
        
        // The Kernel has three global buffer arguments, this assigns the buffers created above to those three arguments
        kernelAdd.setArg (0, bufferA);
        kernelAdd.setArg (1, bufferB);
        kernelAdd.setArg (2, bufferC);
        
        // Now vectorSize versions of the kernel are launched, all getting their unique ID which the kernel queries through get_global_id(0)
        std::cout << "Enqueued kernel computation" << std::endl;
#if VECTOR_ADD_AS_SINGLE_WORK_ITEM
        queue.enqueueTask (kernelAdd, &bufferEvents);
#else
        queue.enqueueNDRangeKernel (kernelAdd, cl::NullRange, cl::NDRange (vectorSize), cl::NullRange, &bufferEvents);
#endif
        
        // A blocking wait until the kernel computation has finished
        queue.finish();
        
        // The kernel placed its computation results into bufferC. Its content is now put back into the host side buffer dest.
        // Contrary to enqueueing the write buffer we chose this operation to be blocking this time as we want to access the data right after this call
        queue.enqueueReadBuffer (bufferC, CL_TRUE, 0, sizeof (int) * vectorSize, dest.data());
        
        // Print the result buffer content
        std::cout << "\nResults:\n";
        for (int r : dest)
            std::cout << r << "; ";
        
        return 0;
    }
    
private:
    static const size_t vectorSize = 20;
};

/**
 * You can call the main without arguments, or can either supply the desired device as argument or the desired platform
 * and device as two arguments. If the desired device or platform cannot be found it will automatically use the default
 * as fallback.
 */
int main (int argc, char** argv)
{
    std::string platform, device;

    if (argc == 3)
    {
        platform = argv[1];
        device   = argv[2];
        std::cout << "Searching for platform " << platform << ", device " << device << std::endl;
    }
    else if (argc == 2)
    {
        device   = argv[1];
        std::cout << "Searching for device " << device << " on default platform" << std::endl;
    }
    
    CLVectorAddDemo clApp (platform, device);
    
    return clApp.runApplicationAndReturnExitCode();
}
