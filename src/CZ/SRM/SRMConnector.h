#ifndef SRMCONNECTOR_H
#define SRMCONNECTOR_H

#include <CZ/SRM/SRMConnectorInterface.h>
#include <CZ/SRM/SRMRenderer.h>
#include <CZ/SRM/SRMObject.h>
#include <CZ/SRM/SRMLog.h>

#include <CZ/Ream/Ream.h>
#include <CZ/Ream/RSubpixel.h>
#include <CZ/Ream/RContentType.h>

#include <CZ/skia/core/SkRegion.h>

#include <CZ/Core/CZWeak.h>
#include <CZ/Core/CZPresentationTime.h>

#include <memory>
#include <string>
#include <xf86drm.h>
#include <xf86drmMode.h>

/**
 * @brief Represents a screen.
 *
 * An @ref SRMConnector abstracts a real or virtual screen where content is shown, such as a laptop panel, HDMI monitor, virtual machine display, etc.
 *
 * ### Rendering
 *
 * Each connector operates on its own dedicated rendering thread, which invokes the callbacks defined in the SRMConnectorInterface passed to initialize().
 *
 * ### Display Modes
 *
 * Most connectors support multiple display modes (@ref SRMConnectorMode), defining resolution and refresh rate.
 * These can be queried via modes() and changed using setMode().
 */
class CZ::SRMConnector : public SRMObject
{
public:

    /**
     * @brief Destructor.
     */
    ~SRMConnector() noexcept;

    /**
     * @brief Returns the DRM connector ID.
     *
     * IDs are unique per device, but not globally unique across multiple devices.
     */
    UInt32 id() const noexcept { return m_id; }

    /**
     * @brief Checks if the connector is initialized.
     */
    bool isInitialized() const noexcept { return m_rend != nullptr; }

    /**
     * @brief Check if the connector is available.
     */
    bool isConnected() const noexcept { return m_isConnected; }

    /**
     * @brief Returns the physical size of the connector in millimeters.
     *
     * The dimensions may be zero, for example, on virtual machine displays.
     */
    SkISize mmSize() const noexcept { return m_mmSize; }

    /**
     * @brief Get the DRM type of the connector.
     *
     * This function returns the DRM type associated with the connector (**DRM_MODE_CONNECTOR_xx** macros defined in `drm_mode.h`).
     *
     * @param connector Pointer to the @ref SRMConnector for which to retrieve the type.
     * @return The DRM type of the connector.
     */
    UInt32 type() const noexcept { return m_type; }

    /**
     * @brief Get a string representation of a connector type.
     *
     * @see type()
     *
     * @return A pointer to the string representation of the connector type or 'unknown'.
     */
    static std::string_view TypeString(UInt32 type) noexcept;

    /**
     * @brief Get the device this connector belongs to.
     *
     * @note The device may not always be the same as the renderer device.
     */
    SRMDevice *device() const noexcept { return m_device; }

    /**
     * @brief Get the name of the connector.
     *
     * The name is always unique, even across devices but could change after a reboot.
     *
     * @return The name of the connector, e.g. HDMI-A-1 or "Unknown" if not available.
     */
    const std::string &name() const noexcept { return m_name; }

    /**
     * @brief Get the manufacturer of the connected display.
     *
     * @return The manufacturer name or "Unknown" if not available.
     */
    const std::string &make() const noexcept { return m_make; }

    /**
     * @brief Get the model of the connected display.
     *
     * @return The model name or "Unknown" if not available.
     */
    const std::string &model() const noexcept { return m_model; }

    /**
     * @brief Get the serial number of the connected display.
     *
     * @return The serial number of the connected display or an empty string if not available.
     */
    const std::string &serial() const noexcept { return m_serial; }

    /**
     * @brief Vector of compatible encoders.
     */
    const std::vector<SRMEncoder*> &encoders() const noexcept { return m_encoders; }

    /**
     * @brief Get the currently used encoder for the connector.
     *
     * @return The current encoder or nullptr if uninitialized.
     */
    SRMEncoder *currentEncoder() const noexcept;

    /**
     * @brief Vecor of available connector modes.
     */
    const std::vector<SRMConnectorMode*> &modes() const noexcept { return m_modes; }

    /**
     * @brief Get the preferred connector mode.
     *
     * This mode typically has the higher resolution and refresh rate.
     *
     * @return The preferred mode or nullptr if disconnected.
     */
    SRMConnectorMode *preferredMode() const noexcept { return m_preferredMode; }

    /**
     * @brief Get the current connector mode.
     *
     * Set to preferredMode() by default.
     *
     * @return The current mode or nullptr if disconnected.
     */
    SRMConnectorMode *currentMode() const noexcept { return m_currentMode; }

    /**
     * @brief Vector of compatible CRTCs.
     */
    const std::vector<SRMCrtc*> &crtcs() const noexcept { return m_crtcs; }

    /**
     * @brief Sets the current mode of the connector.
     *
     * If the connector is initialized the `resized()` event is invoked.
     *
     * @note This method will fail if called from the SRMConnectorInterface callbacks.
     *
     * @return 1 on success, 0 if failed and rolled back to the previous mode and -1 if
     *         rollback also failed (e.g. because the connector was unplugged during the change)
     */
    int setMode(SRMConnectorMode *mode) noexcept;

    /**
     * @brief Get the current CRTC used by the connector.
     *
     * @return The current crtc or nullptr if uninitialized.
     */
    SRMCrtc *currentCrtc() const noexcept;

    /**
     * @brief Get the currently used primary plane.
     *
     * @return The current primary plane or nullptr if uninitialized.
     */
    SRMPlane *currentPrimaryPlane() const noexcept;

    /**
     * @brief Get the current cursor plane.
     *
     * @return The current cursor plane or nullptr if uninitialized or not available.
     */
    SRMPlane *currentCursorPlane() const noexcept;

    /**
     * @brief Checks whether the cursor plane is available and enabled.
     *
     * If false, setCursor() and setCursorPos() will fail.
     */
    bool hasCursor() const noexcept;

    /**
     * @brief Set the pixels of the cursor plane.
     *
     * This function sets the pixels of the hardware cursor for the given @ref SRMConnector.
     *
     * The format of the buffer must be **ARGB8888** with a size of 64x64 pixels.
     * Passing `nullptr` as the buffer hides the cursor.
     */
    bool setCursor(UInt8 *pixels) noexcept;

    /**
     * @brief Set the cursor position.
     *
     * @param po Position in pixel coords relative to the top-left origin of the connector image.
     */
    bool setCursorPos(SkIPoint pos) noexcept;

    /**
     * @brief Checks if the connector is being leased.
     */
    bool leased() const noexcept { return m_leased; }

    /**
     * @brief Initializes a connector, creating its rendering thread and invoking `initializeGL()` once initialized.
     *
     * This function initializes the given @ref SRMConnector. After initialization, calling srmConnectorRepaint()
     * schedules a new rendering frame, which invokes `paintGL()` followed by a `pageFlipped()` event.
     *
     * @param connector Pointer to the @ref SRMConnector to initialize.
     * @param interface Pointer to the @ref SRMConnectorInterface struct to handle events.
     * @param userData Pointer to user data to be associated with the connector.
     * @return 1 if initialization is successful, 0 if an error occurs.
     *
     * @warning Make sure to correctly set up the @ref SRMConnectorInterface to handle all its events, otherwise, it may crash.
     */
    bool initialize(const SRMConnectorInterface *iface, void *data) noexcept;

    /**
     * @brief Schedules a new rendering frame.
     *
     * This function schedules a new rendering frame for the given @ref SRMConnector. Calling this method
     * multiple times during the same frame will not invoke `paintGL()` multiple times, only once.
     * After each `paintGL()` event, this method must be called again to schedule a new frame.
     *
     * @param connector Pointer to the @ref SRMConnector to schedule a new rendering frame.
     * @return 1 if scheduling is successful, 0 otherwise.
     *
     * @note This function triggers paintGL() and pageFlipped() events in the rendering process.
     */
    bool repaint() noexcept;

    /**
     * @brief Uninitializes the connector, triggering `uninitializeGL()`.
     *
     * @returns true on success, and false if its already uninitialized or if called from its rendering thread.
     */
    bool uninitialize() noexcept;

    /**
     * @brief Returns the buffer of a specific framebuffer index, usable as a texture for rendering.
     *
     * This function returns the buffer of the specified framebuffer index of the given @ref SRMConnector.
     * This buffer can be used as a texture for rendering, but it may not always be supported. Always check if `NULL` is returned.
     *
     * Additionally, note that the buffer may not always be shared among all GPUs. In such cases, calling srmBufferGetTextureID() may return 0.
     *
     * @param connector Pointer to the @ref SRMConnector for which to retrieve the framebuffer buffer.
     * @param bufferIndex The index of the framebuffer buffer to retrieve.
     * @return A pointer to the @ref SRMBuffer if available, or `NULL` if not supported.
     */
    const std::vector<std::shared_ptr<RImage>> &images() const noexcept;

    /**
     * @brief Returns the current framebuffer index.
     *
     * This function returns the index of the current framebuffer where the rendered content is stored
     * during a `paintGL()` event.
     *
     * @param connector Pointer to the @ref SRMConnector to query for the current framebuffer index.
     * @return The current framebuffer index.
     */
    UInt32 imageIndex() const noexcept;

    /**
     * @brief Retrieves the age of the current buffer.
     *
     * This function returns the age of the buffer as specified in the
     * [EGL_EXT_buffer_age](https://registry.khronos.org/EGL/extensions/EXT/EGL_EXT_buffer_age.txt)
     * extension specification.
     */
    UInt32 imageAge() const noexcept;

    std::shared_ptr<RImage> currentImage() const noexcept;

    /**
     * @brief Get the subpixel layout associated with a connector.
     *
     * This function retrieves the subpixel layout associated with a given connector.
     * Subpixels are individual color elements that make up a pixel on a display, and the subpixel layout
     * is crucial for accurate color interpretation and display. The returned value indicates how the
     * red, green, and blue subpixels are arranged in relation to each other.
     *
     * @note The returned value is only meaningful when a display is connected. If the subpixel layout is unknown or not applicable,
     *       the method may return @ref SRM_CONNECTOR_SUBPIXEL_UNKNOWN or @ref SRM_CONNECTOR_SUBPIXEL_NONE, respectively.
     *
     * @param connector Pointer to the @ref SRMConnector for which the subpixel layout is requested.
     * @return The subpixel layout associated with the connector.
     *
     */
    RSubpixel subpixel() const noexcept { return m_subpixel; }

    /**
     * @brief Gets the number of elements used to represent each RGB gamma correction curve.
     *
     * This function retrieves the number of elements (N) used to represent each RGB gamma correction curve,
     * where N is the count of @ref UInt16 elements for red, green, and blue curves.
     *
     * @note This method should only be called once the connector is assigned a CRTC,
     *       meaning after the connector is initialized. If called when uninitialized, it returns 0.
     *       If the connector is initialized and this method returns 0, it indicates that the driver
     *       does not support gamma correction.
     *
     * @param connector Pointer to the @ref SRMConnector.
     * @return The number of elements used to represent each RGB gamma correction curve, or 0 if the connector is uninitialized or
     *         if the driver does not support gamma correction.
     */
    UInt64 gammaSize() const noexcept;

    /**
     * @brief Sets the gamma table.
     *
     * Defaults to linear.
     *
     * @param gammaLUT The gamma lut to set, or nullptr to set it linear.
     * @return true on success, false on failure (uninitialized connector or no gamma correction support).
     */
    bool setGammaLUT(std::shared_ptr<const RGammaLUT> gammaLUT) noexcept;

    // nullptr if not available or the connector is uninitialized
    std::shared_ptr<const RGammaLUT> gammaLUT() const noexcept;

    bool canDisableVSync() const noexcept;

    /**
     * @brief Returns the current vsync status.
     *
     * Returns 1 if vsync is enabled, 0 otherwise. V-Sync is enabled by default.
     *
     * @param connector The @ref SRMConnector instance.
     * @return 1 if vsync is enabled, 0 if not.
     */
    bool isVSyncEnabled() const noexcept { return m_vsync; }

    /**
     * @brief Toggles vsync.
     *
     * Disabling vsync is only allowed if srmConnectorHasVSyncControlSupport() returns 1.
     * VSync is enabled by default.
     *
     * @note Disabling vsync for the atomic API is supported only starting from Linux version 6.8.
     *       If you wish to enforce the use of the legacy API, set the **SRM_FORCE_LEGACY_API** environment variable to 1.
     *
     * @param connector The @ref SRMConnector instance.
     * @param enabled Set to 1 to enable vsync, 0 to disable vsync.
     * @return 1 if the vsync change was successful, 0 otherwise.
     */
    bool enableVSync(bool enabled) noexcept;

    /**
     * @brief Paint event id.
     *
     * This ID can be used to detect which specific paintGL() event corresponds
     * to a given presentation time.
     *
     * @return Always returns 0 if the connector is uninitialized.
     */
    UInt64 paintEventId() const noexcept;

    /**
     * @brief Sets a hint of the content type being displayed.
     *
     * @see @ref SRM_CONNECTOR_CONTENT_TYPE
     *
     * @note The default value is @ref SRM_CONNECTOR_CONTENT_TYPE_GRAPHICS
     *
     * @param connector A pointer to the @ref SRMConnector instance.
     * @param contentType The content type hint.
     */
    void setContentType(RContentType type, bool force = false) noexcept;

    /**
     * @brief Gets the content type hint.
     *
     * @see srmConnectorSetContentType()
     *
     * @param connector A pointer to the @ref SRMConnector instance.
     */
    RContentType contentType() const noexcept { return m_contentType; }

    /**
     * @brief Sets a custom scanout buffer for the primary plane.
     *
     * This function allows you to set a custom scanout buffer for the primary plane only during a single frame.
     * It must called within a `paintGL()` event. Calling it outside a `paintGL()` will result in an error.
     *
     * If successfully set, the current buffer index is not updated, and no OpenGL rendering operations should be performed within the `paintGL()` event.
     * If not set again in subsequent frames, the connector's framebuffers are restored, and the current buffer index continues to be updated as usual.
     *
     * @note The size of the buffer must match the dimensions of the current connector's mode.
     *
     * If successfully set, the internal reference counter of the buffer is increased, ensuring it remains scannable for at least the given frame
     * even if it is destroyed.
     *
     * @note If `SRM_DISABLE_CUSTOM_SCANOUT` is set to 1, this function always return 0.
     *
     * @param connector A pointer to the @ref SRMConnector instance.
     * @param buffer The buffer to scan, or `NULL` to restore the default connector framebuffers.
     * @return 1 if the custom buffer will be scanned, 0 otherwise.
     */
    bool setCustomScanoutImage(std::shared_ptr<RImage> image) noexcept;

    /**
     * @brief Checks if the connector is not intended for desktop usage.
     *
     * Some connectors, such as VR headsets, set this property to 1 to indicate they are not meant for desktop use.
     *
     * @return 1 if not meant for desktop usage, 0 if meant for desktop or if disconnected.
     */
    bool isNonDesktop() const noexcept { return m_nonDesktop; };

    /**
     * @brief Locks the buffer currently being displayed and ignores srmConnectorRepaint() calls.
     *
     * When set to 1, no `paintGL()` or `pageFlipped()` events are triggered. The default value is 0.
     *
     * @param connector The @ref SRMConnector instance.
     * @param locked    A value of 1 locks the current buffer, 0 unlocks it.
     */
    void lockCurrentBuffer(bool lock) noexcept;

    /**
     * @brief Retrieves the locked state of the current buffer.
     *
     * @see srmConnectorSetCurrentBufferLocked()
     *
     * @param connector The @ref SRMConnector instance.
     * @return          A value of 1 indicates the current buffer is locked, 0 indicates it is unlocked.
     */
    bool isCurrentBufferLocked() const noexcept;

    /**
     * @brief Finds a compatible display configuration for the connector.
     *
     * Attempts to find an encoder, CRTC, primary plane, and cursor plane
     * suitable for this connector. The search may succeed even if
     * a cursor plane is not found.
     *
     * @param[out] bestEncoder       Chosen encoder.
     * @param[out] bestCrtc          Chosen CRTC.
     * @param[out] bestPrimaryPlane  Chosen primary plane.
     * @param[out] bestCursorPlane   Chosen cursor plane, or nullptr if none.
     * @return true if a valid configuration was found, false otherwise.
     */
    bool findConfiguration(SRMEncoder **bestEncoder, SRMCrtc **bestCrtc, SRMPlane **bestPrimaryPlane, SRMPlane **bestCursorPlane) noexcept;

    /**
     * @brief Damaged region during a paintGL event.
     *
     * Should be filled with the area updated during paintGL(), helping the graphics backend
     * to minimize the number of pixels copied (raster backend, hybrid GPU setups, etc).
     *
     * Defined in image-local coordinates. Automatically reset to full damage ((0, 0), currentImage().size()) before each paintGL() call.
     */
    SkRegion damage;

    CZLogger log { SRMLog };

private:
    friend class SRMCore;
    friend class SRMDevice;
    friend class SRMRenderer;
    friend class SRMLease;

    static SRMConnector *Make(UInt32 id, SRMDevice *device) noexcept;
    SRMConnector(UInt32 id, SRMDevice *device) noexcept :
        m_id(id),
        m_device(device)
    {}

    bool updateProperties(drmModeConnectorPtr res) noexcept;
    bool updateNames(drmModeConnectorPtr res) noexcept;
    bool updateEncoders(drmModeConnectorPtr res) noexcept;
    bool updateModes(drmModeConnectorPtr res) noexcept;

    bool unlockRenderer(bool repaint) noexcept;

    SRMConnectorMode *findPreferredMode() const noexcept;

    void destroyModes() noexcept;

    std::unique_ptr<SRMRenderer> m_rend;

    UInt32 m_id {};
    UInt32 m_nameId {}; // Used to name the connector e.g HDMI-A-<0>
    UInt32 m_type {}; // DRM connector type

    SRMDevice *m_device { nullptr };
    RSubpixel m_subpixel { RSubpixel::Unknown };
    RContentType m_contentType { RContentType::Graphics };
    SkISize m_mmSize {};

    bool m_isConnected {};
    bool m_nonDesktop {};
    bool m_vsync { true };
    bool m_leased {};

    CZWeak<SRMConnectorMode> m_currentMode;
    CZWeak<SRMConnectorMode> m_preferredMode;
    std::vector<SRMConnectorMode*> m_modes;
    std::vector<SRMCrtc*> m_crtcs;
    std::vector<SRMEncoder*> m_encoders;

    std::string m_name;
    std::string m_make;
    std::string m_model;
    std::string m_serial;

    struct PropIDs
    {
        UInt32
            CRTC_ID,
            DPMS,
            EDID,
            PATH,
            link_status,
            non_desktop,
            content_type,
            panel_orientation,
            subconnector,
            vrr_capable;
    } m_propIDs {};
};

#endif // SRMCONNECTOR_H
