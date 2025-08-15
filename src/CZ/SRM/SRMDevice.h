#ifndef SRMDEVICE_H
#define SRMDEVICE_H

#include <CZ/SRM/SRMObject.h>
#include <CZ/SRM/SRMLog.h>
#include <CZ/Ream/RDevice.h>
#include <CZ/CZBitset.h>
#include <mutex>
#include <string>
#include <xf86drmMode.h>

/**
 * @brief Representation of an open DRM device, typically a GPU.
 *
 * An @ref SRMDevice represents a DRM device, typically a GPU, which can contain multiple connectors
 * and a specific rendering mode (**SELF**, **PRIME**, **DUMB** or **CPU**).
 */
class CZ::SRMDevice final : public SRMObject
{
public:

    struct ClientCaps
    {
        bool Stereo3D;
        bool UniversalPlanes;

        /**
         * @brief Get the client DRM driver's support for Atomic API capability.
         *
         * @param device A pointer to the @ref SRMDevice instance.
         *
         * @return 1 if the client DRM driver supports Atomic API capability, 0 otherwise.
         *
         * @note Setting the **SRM_FORCE_LEGACY_API** environment variable to 1, disables the Atomic API.
         */
        bool Atomic;
        bool AspectRatio;
        bool WritebackConnectors;
    };

    struct Caps
    {
        bool DumbBuffer;
        bool PrimeImport;
        bool PrimeExport;
        bool AddFb2Modifiers;

        /**
         * @brief Get the driver's support for Async Page Flip capability.
         *
         * @note This specifically indicates support for the legacy API, no the atomic API @see srmDeviceGetCapAtomicAsyncPageFlip().
         *
         * @param device A pointer to the @ref SRMDevice instance.
         *
         * @return 1 if the DRM device driver supports Async Page Flip capability, 0 otherwise.
         */
        bool AsyncPageFlip;

        /**
         * @brief Get the driver's support for Atomic Async Page Flip capability.
         *
         * @note Atomic async page flips are supported since Linux 6.8.
         *
         * @param device A pointer to the @ref SRMDevice instance.
         *
         * @return 1 if the DRM device driver supports Atomic Async Page Flip capability, 0 otherwise.
         */
        bool AtomicAsyncPageFlip;

        /**
         * @brief Retrieve the driver's support for monotonic timestamps.
         *
         * @see srmConnectorGetPresentationClock()
         * @see srmConnectorGetPresentationTime()
         *
         * @param device A pointer to the @ref SRMDevice instance.
         *
         * @return 1 if the DRM device reports vblank timestamps with CLOCK_MONOTONIC, 0 if it uses CLOCK_REALTIME.
         */
        bool TimestampMonotonic;
    };

    /**
     * @brief Get the @ref SRMCore instance to which this device belongs.
     *
     * @param device A pointer to the @ref SRMDevice instance.
     *
     * @return A pointer to the @ref SRMCore instance that this device belongs to.
     */
    SRMCore *core() const noexcept { return m_core; }

    /**
     * @brief Get the DRM device name (e.g., `/dev/dri/card0`) associated with this device.
     *
     * @param device A pointer to the @ref SRMDevice instance.
     *
     * @return The name of the DRM device associated with this @ref SRMDevice.
     */
    const std::string &nodePath() const noexcept { return m_nodePath; }
    const std::string &nodeName() const noexcept { return m_nodeName; }
    const ClientCaps &clientCaps() const noexcept { return m_clientCaps; }
    const Caps &caps() const noexcept { return m_caps; }

    /**
     * @brief Get the file descriptor of the DRM device associated with this device.
     *
     * @param device A pointer to the @ref SRMDevice instance.
     *
     * @return The file descriptor of the DRM device.
     */
    int fd() const noexcept { return m_fd; }

    /**
     * @brief Get a list of connectors of this device.
     *
     * @param device A pointer to the @ref SRMDevice instance.
     *
     * @return A list of pointers to the connectors (@ref SRMConnector) associated with this device.
     */
    const std::vector<SRMConnector*> &connectors() const noexcept { return m_connectors; }

    /**
     * @brief Get a list of planes of this device.
     *
     * @param device A pointer to the @ref SRMDevice instance.
     *
     * @return A list of pointers to the planes (@ref SRMPlane) associated with this device.
     */
    const std::vector<SRMPlane*> &planes() const noexcept { return m_planes; }

    /**
     * @brief Get a list of CRTCs (Cathode Ray Tube Controllers) of this device.
     *
     * @param device A pointer to the @ref SRMDevice instance.
     *
     * @return A list of pointers to the CRTCs (@ref SRMCrtc) associated with this device.
     */
    const std::vector<SRMCrtc*> &crtcs() const noexcept { return m_crtcs; }

    /**
     * @brief Get a list of encoders of this device.
     *
     * @param device A pointer to the @ref SRMDevice instance.
     *
     * @return A list of pointers to the encoders (@ref SRMEncoder) associated with this device.
     */
    const std::vector<SRMEncoder*> &encoders() const noexcept { return m_encoders; }

    bool isBootVGA() const noexcept { return m_isBootVGA; }

    RDevice *reamDevice() const noexcept { return m_reamDevice; }

    ~SRMDevice() noexcept;

    CZLogger log { SRMLog };

private:
    friend class SRMCore;
    friend class SRMRenderer;
    friend class SRMConnector;
    static SRMDevice *Make(SRMCore *core, const char *nodePath, bool isBootVGA) noexcept;
    SRMDevice(SRMCore *core, const char *nodePath, bool isBootVGA) noexcept;
    bool init() noexcept;
    bool initClientCaps() noexcept;
    bool initCaps() noexcept;
    bool initCrtcs(drmModeResPtr res) noexcept;
    bool initEncoders(drmModeResPtr res) noexcept;
    bool initPlanes() noexcept;
    bool initConnectors(drmModeResPtr res) noexcept;

    bool dispatchHotplugEvents() noexcept;

    enum class PDriver
    {
        unknown,
        i915,
        nouveau,
        nvidia,
        lima
    };

    CZWeak<RDevice> m_reamDevice;

    int m_fd { -1 };
    std::string m_nodePath;
    std::string m_nodeName;
    SRMCore *m_core {};
    PDriver m_driver { PDriver::unknown };

    Caps m_caps {};
    bool m_isBootVGA {};
    bool m_rescanConnectors {};
    ClientCaps m_clientCaps {};
    clockid_t m_clock { CLOCK_REALTIME };

    std::vector<SRMConnector*> m_connectors;
    std::vector<SRMPlane*> m_planes;
    std::vector<SRMCrtc*> m_crtcs;
    std::vector<SRMEncoder*> m_encoders;

    // Prevents multiple calls to drmModeHandleEvent
    std::recursive_mutex m_pageFlipMutex;
};

#endif // SRMDEVICE_H
