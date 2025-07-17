#ifndef SRMCRTC_H
#define SRMCRTC_H

#include <CZ/SRM/SRMObject.h>
#include <xf86drmMode.h>

/**
 * @brief Cathode Ray Tube Controllers (CRTCs).
 *
 * A Cathode Ray Tube Controller (CRTC) is responsible for controlling the display timings and attributes
 * for a connected display device. This module provides functions to work with CRTCs in a SRM context.
 *
 * @note This module is primarily used by SRM internally and may not be of much use to users.
 */
class CZ::SRMCrtc final : public SRMObject
{
public:
    /**
     * @brief Get the DRM ID of this CRTC.
     *
     * @param crtc A pointer to the @ref SRMCrtc instance.
     *
     * @return The DRM ID of the CRTC.
     */
    UInt32 id() const noexcept { return m_id; }

    /**
     * @brief Get the device this CRTC belongs to.
     *
     * @param crtc A pointer to the @ref SRMCrtc instance.
     *
     * @return A pointer to the @ref SRMDevice that this CRTC belongs to.
     */
    SRMDevice *device() const noexcept { return m_device; }

    /**
     * @brief Returns a pointer to the @ref SRMConnector that is currently using this CRTC,
     *        or `NULL` if it is not used by any connector.
     *
     * @param crtc A pointer to the @ref SRMCrtc instance.
     *
     * @return A pointer to the @ref SRMConnector currently using this CRTC, or `NULL` if not in use.
     */
    SRMConnector *currentConnector() const noexcept { return m_currentConnector; }

    /**
     * @brief Gets the number of elements used to represent each RGB gamma correction curve.
     *
     * This function retrieves the number of elements (N) used to represent each RGB gamma correction curve,
     * where N is the count of @ref UInt16 elements for red, green, and blue curves.
     *
     * @see srmConnectorSetGamma()
     *
     * @param connector Pointer to the @ref SRMCrtc.
     * @return The number of elements used to represent each RGB gamma correction curve, or 0
     *         if the driver does not support gamma correction.
     */
    UInt64 gammaSize() const noexcept;
private:
    friend class SRMDevice;
    friend class SRMConnector;
    friend class SRMRenderer;
    static SRMCrtc *Make(UInt32 id, SRMDevice *device) noexcept;
    SRMCrtc(UInt32 id, SRMDevice *device) noexcept :
        m_id(id),
        m_device(device)
    {}
    bool initPropIds() noexcept;
    bool initLegacyGamma(drmModeCrtcPtr res) noexcept;
    UInt32 m_id;
    SRMDevice *m_device { nullptr };
    SRMConnector *m_currentConnector { nullptr };
    UInt64 m_gammaSizeLegacy { 0 };
    UInt64 m_gammaSize { 0 };
    struct PropIDs
    {
        UInt32
            ACTIVE,
            GAMMA_LUT,
            GAMMA_LUT_SIZE,
            MODE_ID,
            VRR_ENABLED;
    } m_propIDs;
};

#endif // SRMCRTC_H
