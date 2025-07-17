#ifndef SRMENCODER_H
#define SRMENCODER_H

#include <CZ/SRM/SRMObject.h>
#include <CZ/CZWeak.h>
#include <xf86drmMode.h>

/**
 * @brief Connector encoder in a DRM context.
 *
 * An @ref SRMEncoder represents a connector encoder device responsible for driving displays (e.g., monitors or screens).
 * This module provides functions to work with display encoders, including retrieving their information
 * and associated resources.
 *
 * @note This module is primarily used by SRM internally and may not be of much use to external users.
 */

class CZ::SRMEncoder final : public SRMObject
{
public:
    /**
     * @brief Get the unique identifier of the encoder.
     *
     * @param encoder A pointer to the @ref SRMEncoder instance.
     *
     * @return The ID of the encoder.
     */
    UInt32 id() const noexcept { return m_id; }

    /**
     * @brief Get the device to which this encoder belongs.
     *
     * @param encoder A pointer to the @ref SRMEncoder instance.
     *
     * @return A pointer to the @ref SRMDevice representing the device to which the encoder belongs.
     */
    SRMDevice *device() const noexcept { return m_device; }

    /**
     * @brief Get a list of CRTCs (Cathode Ray Tube Controllers) compatible with this encoder.
     *
     * @param encoder A pointer to the @ref SRMEncoder instance.
     *
     * @return A list of pointers to the CRTCs (@ref SRMCrtc) compatible with the encoder.
     */
    const std::vector<SRMCrtc*> &crtcs() const noexcept { return m_crtcs; }

    /**
     * @brief Get the connector that is currently using this encoder.
     *
     * @param encoder A pointer to the @ref SRMEncoder instance.
     *
     * @return A pointer to the @ref SRMConnector currently utilizing this encoder, or `NULL` if not in use.
     */
    SRMConnector *currentConnector() const noexcept { return m_currentConnector; };
private:
    friend class SRMDevice;
    friend class SRMConnector;
    friend class SRMRenderer;
    static SRMEncoder *Make(UInt32 id, SRMDevice *device) noexcept;
    SRMEncoder(UInt32 id, SRMDevice *device) noexcept :
        m_id(id),
        m_device(device)
    {}
    bool initCrtcs(drmModeEncoderPtr res) noexcept;
    UInt32 m_id;
    SRMDevice *m_device { nullptr };
    CZWeak<SRMConnector> m_currentConnector;
    std::vector<SRMCrtc*> m_crtcs;
};

#endif // SRMENCODER_H
