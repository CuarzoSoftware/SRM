#ifndef SRMCORE_H
#define SRMCORE_H

#include <CZ/SRM/SRMObject.h>
#include <CZ/Core/CZSignal.h>
#include <CZ/Core/CZBitset.h>
#include <CZ/Core/CZSpFd.h>
#include <memory>
#include <thread>
#include <unordered_set>

struct udev;
struct udev_monitor;

namespace CZ
{
    class RCore;

    /**
     * @brief SRMCore interface.
     *
     * @see SRMCore::Make()
     */
    struct SRMInterface
    {
        /**
         * Function to open a DRM device. Must return the opened DRM device file descriptor.
         *
         * @param path The path to the DRM device file (e.g., `/dev/dri/card0`).
         * @param flags Flags for opening the DRM device (e.g., `O_RDWR`).
         * @param data A pointer to the user data provided in srmCoreCreate().
         * @return The file descriptor of the opened DRM device.
         */
        int (*openRestricted)(const char *path, int flags, void *data);

        /**
         * Function to close a DRM device file descriptor.
         *
         * @param fd The file descriptor of the DRM device to close.
         * @param data A pointer to the user data provided in srmCoreCreate().
         */
        void (*closeRestricted)(int fd, void *data);
    };
};

/**
 * @brief Core class.
 *
 * Manages and provides access to all available DRM devices.
 */
class CZ::SRMCore final : public SRMObject
{
public:
    /**
     * @brief Creates an SRMCore instance.
     *
     * Creates a new SRMCore instance, which will scan and open all available primary DRM devices
     * using the provided interface.
     *
     * @param interface A pointer to the @ref SRMInterface that provides access to DRM devices.
     * @param userData  A pointer to the user data to associate with the @ref SRMCore instance.
     *
     * @return A shared pointer to the created @ref SRMCore instance, or `nullptr` on failure.
     */
    static std::shared_ptr<SRMCore> Make(const SRMInterface *iface, void *userData) noexcept;

    /**
     * @brief Creates an SRMCore instance from a set of already opened DRM device file descriptors.
     *
     * @return A shared pointer to the created @ref SRMCore instance, or `nullptr` on failure.
     */
    static std::shared_ptr<SRMCore> Make(std::unordered_set<CZSpFd> &&fds) noexcept;

    /**
     * @brief Suspends SRMCore.
     *
     * This function should be called when devices lose DRM master status
     * (e.g., due to a TTY switch). It emits synthetic `onConnectorUnplugged` events
     * for each currently available connector to simulate disconnection.
     *
     * While suspended, no KMS operations can be performed.
     *
     * @param core A pointer to the @ref SRMCore instance.
     * @return true on success or if already suspended, false on failure.
     */
    bool suspend() noexcept;

    /**
     * @brief Resumes SRMCore.
     *
     * This function should be called when devices regain DRM master status.
     * It also emits `onConnectorPlugged` for each available connector.
     *
     * @return true if the core was successfully resumed or was already active, false if the operation failed.
     */
    bool resume() noexcept;

    /**
     * @brief Check if SRMCore is currently suspended.
     */
    bool isSuspended() noexcept { return m_isSuspended; }

    /**
     * @brief Get a pollable udev monitor file descriptor for listening to hotplug events.
     *
     * @see dispatch()
     *
     * @note The fd is owned by SRM, do not close it.
     */
    int fd() const noexcept;

    /**
     * @brief Dispatch pending udev monitor events or block until an event occurs or the timeout is reached.
     *
     * Passing a timeout value of -1 makes the function block indefinitely until an event occurs.
     *
     * @return (>= 0) on success, or -1 on error.
     */
    int dispatch(int timeoutMs) noexcept;

    /**
     * @brief Vector of available devices.
     */
    const std::vector<SRMDevice*> &devices() const noexcept { return m_devices; }

    /**
     * @brief Determines whether legacy IOCTLs are forcibly used to drive cursors.
     *
     * This behavior is controlled by the environment variable
     * `CZ_SRM_FORCE_LEGACY_CURSOR`, which accepts values `0` (disabled) or `1` (enabled).
     *
     * @return true if legacy IOCTLs are being forced, false otherwise.
     */
    bool forcingLegacyCursor() const noexcept { return m_forceLegacyCursor; }

    /**
     * @brief Emitted when a connector is plugged in.
     */
    CZSignal<SRMConnector*> onConnectorPlugged;

    /**
     * @brief Emitted when a connector is unplugged.
     */
    CZSignal<SRMConnector*> onConnectorUnplugged;

    /**
     * @brief Destructor.
     *
     * All connectors are automatically uninitialized before destruction.
     */
    ~SRMCore() noexcept;

private:
    friend class SRMDevice;
    friend class SRMConnector;
    friend class SRMRenderer;
    SRMCore(const SRMInterface *iface, void *userData) noexcept :
        m_iface(iface), m_ifaceData(userData) {}

    SRMCore(std::unordered_set<CZSpFd> &&fds) noexcept :
        m_fds(std::move(fds)) {}

    bool init() noexcept;
    bool initUdev() noexcept;
    bool initDevices() noexcept;
    bool initMonitor() noexcept;
    bool initReam() noexcept;

    void unplugAllConnectors() noexcept;
    bool isRenderThread(std::thread::id threadId) noexcept;

    udev *m_udev {};
    udev_monitor *m_monitor {};
    std::vector<SRMDevice*> m_devices;
    bool m_isSuspended {};
    bool m_forceLegacyCursor {};
    bool m_disableCursor {};
    bool m_disableScanout {};
    std::shared_ptr<RCore> m_ream;

    const SRMInterface *m_iface { nullptr };
    void *m_ifaceData { nullptr };

    std::unordered_set<CZSpFd> m_fds;
};

#endif // SRMCORE_H
