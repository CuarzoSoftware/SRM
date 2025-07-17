#ifndef SRMCORE_H
#define SRMCORE_H

#include <CZ/SRM/SRMObject.h>
#include <CZ/CZSignal.h>
#include <CZ/CZBitset.h>
#include <memory>

struct udev;
struct udev_monitor;

namespace CZ
{
    class RCore;

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

class CZ::SRMCore final : public SRMObject
{
public:
    /**
     * @brief Creates a new @ref SRMCore instance.
     *
     * Creates a new @ref SRMCore instance, which will scan and open all available DRM devices
     * using the provided interface, find the best allocator device, and configure its rendering modes.
     *
     * @param interface A pointer to the @ref SRMInterface that provides access to DRM devices.
     * @param userData  A pointer to the user data to associate with the @ref SRMCore instance.
     *
     * @return A pointer to the newly created @ref SRMCore instance on success, or `NULL` on failure.
     *
     * @note The caller is responsible for releasing the @ref SRMCore instance using srmCoreDestroy() when no longer needed.
     */
    static std::shared_ptr<SRMCore> Make(const SRMInterface *iface, void *userData) noexcept;

    /**
     * @brief Temporarily suspends SRM.
     *
     * This function temporarily suspends all connector rendering threads and evdev events within SRM.\n
     * It should be used when switching to another session in a multi-session system.\n
     * While the core is suspended, SRM no longer acts as the DRM master, and KMS operations cannot be performed.\n
     * For guidance on enabling multi-session functionality using libseat, please refer to the [srm-multi-session](md_md__examples.html) example.
     *
     * @note Pending hotplugging events will be emitted once the core is resumed.
     *
     * @param core A pointer to the @ref SRMCore instance.
     * @return Returns 1 on success and 0 if the operation fails.
     */
    bool suspend() noexcept;

    /**
     * @brief Resumes SRM.
     *
     * This function resumes a previously suspended @ref SRMCore, allowing connectors rendering threads
     * and evdev events to continue processing. It should be used after calling srmCoreSuspend()
     * to bring the @ref SRMCore back to an active state.
     *
     * @param core A pointer to the @ref SRMCore instance to resume.
     * @return Returns 1 on success and 0 if the operation fails.
     */
    bool resume() noexcept;

    /**
     * @brief Check if @ref SRMCore is currently suspended.
     *
     * This function checks whether an @ref SRMCore instance is currently in a suspended state,
     * meaning that connector rendering threads and evdev events are temporarily halted.
     *
     * @param core A pointer to the @ref SRMCore instance to check.
     * @return Returns 1 if the @ref SRMCore is suspended, and 0 if it is active.
     */
    bool isSuspended() noexcept { return m_pf.has(pSuspended); }

    /**
     * @brief Get a pollable udev monitor file descriptor for listening to hotplugging events.
     *
     * The returned fd can be used to monitor devices and connectors hotplugging events using polling mechanisms.\n
     * Use srmCoreProcessMonitor() to dispatch pending events.
     *
     * @param core A pointer to the @ref SRMCore instance.
     *
     * @return The file descriptor for monitoring hotplugging events.
     */
    int fd() const noexcept { return m_fd; }

    /**
     * @brief Dispatch pending udev monitor events or block until an event occurs or a timeout is reached.
     *
     * Passing a timeout value of -1 makes the function block indefinitely until an event occurs.
     *
     * @param core       A pointer to the @ref SRMCore instance.
     * @param msTimeout  The timeout value in milliseconds. Use -1 to block indefinitely.
     *
     * @return (>= 0) on success, or -1 on error.
     */
    int dispatch(int timeoutMs) noexcept;

    /**
     * @brief Get a list of all available devices (@ref SRMDevice).
     *
     * @param core A pointer to the @ref SRMCore instance.
     *
     * @return A list containing all available @ref SRMDevice instances.
     */
    const std::vector<SRMDevice*> &devices() const noexcept { return m_devices; }

    /**
     * @brief Registers a new listener to be invoked when a new connector is plugged.
     *
     * @param core     A pointer to the @ref SRMCore instance.
     * @param callback A callback function to be called when a new connector is plugged.
     * @param userData A pointer to user-defined data to be passed to the callback.
     *
     * @return A pointer to the newly registered @ref SRMListener instance for connector plugged events.
     */
    CZSignal<SRMConnector*> onConnectorPlugged;

    /**
     * @brief Registers a new listener to be invoked when an already plugged connector is unplugged.
     *
     * @param core     A pointer to the @ref SRMCore instance.
     * @param callback A callback function to be called when a connector is unplugged.
     * @param userData A pointer to user-defined data to be passed to the callback.
     *
     * @return A pointer to the newly registered @ref SRMListener instance for connector unplugged events.
     */
    CZSignal<SRMConnector*> onConnectorUnplugged;

    ~SRMCore() noexcept;

private:
    friend class SRMDevice;
    friend class SRMConnector;
    friend class SRMRenderer;
    SRMCore(const SRMInterface *iface, void *userData) noexcept :
        m_iface(iface), m_ifaceData(userData) {}

    bool init() noexcept;
    bool initUdev() noexcept;
    bool initDevices() noexcept;
    bool initMonitor() noexcept;
    bool initReam() noexcept;

    enum PF : UInt8
    {
        pSuspended         = static_cast<UInt8>(1) << 0,
        pForceLegacyCursor = static_cast<UInt8>(1) << 1,
        pDisableCursor     = static_cast<UInt8>(1) << 2,
        pDisableScanout    = static_cast<UInt8>(1) << 3
    };

    int m_fd { -1 }; // EPOLL
    CZBitset<PF> m_pf;
    std::shared_ptr<RCore> m_ream;
    const SRMInterface *m_iface { nullptr };
    void *m_ifaceData { nullptr };
    udev *m_udev {};
    udev_monitor *m_monitor {};
    std::vector<SRMDevice*> m_devices;
};

#endif // SRMCORE_H
