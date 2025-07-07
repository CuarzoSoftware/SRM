#ifndef SRMTYPES_H
#define SRMTYPES_H

#include <string_view>
#include <CZ/CZ.h>

#define SRM_VERSION_MAJOR 1
#define SRM_VERSION_MINOR 0
#define SRM_VERSION_PATCH 0
#define SRM_VERSION_BUILD 1
#define SRM_MAX_BUFFERING 3

namespace CZ
{
    /**
     * @brief Rendering mode supported by an SRM device, listed from best to worst.
     */
    enum class SRMMode
    {
        /// The GPU can directly render into its own connectors
        Self,

        /// Another GPU renders into a PRIME buffer, which is then imported and directly scanned out or blitted into a scannable buffer.
        Prime,

        /// Another GPU renders into an offscreen buffer, which is then copied into a DUMB buffer (GPU to CPU copy).
        Dumb,

        /// Another GPU renders into an offscreen buffer, which is then copied into main memory. A renderable texture is created from the pixels and blitted into a scannable buffer (GPU to CPU + CPU to GPU copy).
        CPU
    };

    /**
     * @brief Get a string representation of a rendering mode.
     */
    const std::string_view &SRMModeString(SRMMode type) noexcept;

    class SRMObject;
    class SRMCore;
    class SRMDevice;
    class SRMEncoder;
    class SRMCrtc;
    class SRMConnector;
    class SRMConnectorMode;
    class SRMPlane;
};

#endif // SRMTYPES_H
