#ifndef SRMCONNECTORMODE_H
#define SRMCONNECTORMODE_H

#include <CZ/skia/core/SkSize.h>
#include <CZ/SRM/SRMObject.h>
#include <xf86drmMode.h>

/**
 * @brief Resolution and refresh rate configuration for a connector.
 *
 * An @ref SRMConnectorMode represents the resolution and refresh rate settings for a connector.\n
 * Each @ref SRMConnector can have multiple modes, which can be listed using srmConnectorGetModes()
 * and selected using srmConnectorSetMode();
 */
class CZ::SRMConnectorMode final : public SRMObject
{
public:
    SRMConnectorMode(SRMConnector *connector, const drmModeModeInfo *info) noexcept :
        m_connector(connector), m_info(*info) {}

    /**
     * @brief Get the connector associated with the connector mode.
     *
     * This function returns the @ref SRMConnector to which the given @ref SRMConnectorMode belongs.
     *
     * @param connectorMode Pointer to the @ref SRMConnectorMode for which to retrieve the associated connector.
     * @return Pointer to the @ref SRMConnector associated with the connector mode.
     */
    SRMConnector *connector() const noexcept { return m_connector; }

    /**
     * @brief Get the width of the connector mode.
     *
     * This function returns the width in pixels of the resolution associated with the given @ref SRMConnectorMode.
     *
     * @param connectorMode Pointer to the @ref SRMConnectorMode for which to retrieve the width.
     * @return The width of the connector mode.
     */
    SkISize size() const noexcept { return SkISize::Make(info().hdisplay, info().vdisplay); }

    /**
     * @brief Get the refresh rate of the connector mode.
     *
     * This function returns the refresh rate in Hertz (Hz) associated with the given @ref SRMConnectorMode.
     *
     * @param connectorMode Pointer to the @ref SRMConnectorMode for which to retrieve the refresh rate.
     * @return The refresh rate of the connector mode.
     */
    UInt32 refreshRate() const noexcept { return info().vrefresh; }

    /**
     * @brief Check if the connector mode is the preferred mode by the connector.
     *
     * This function checks if the given @ref SRMConnectorMode is the preferred mode for the associated @ref SRMConnector.
     *
     * @param connectorMode Pointer to the @ref SRMConnectorMode to check if it is the preferred mode.
     * @return 1 if the connector mode is preferred, 0 otherwise.
     */
    bool isPreferred() const noexcept;

    const drmModeModeInfo &info() const noexcept { return m_info; }
private:
    friend class SRMConnector;
    SRMConnector *m_connector { nullptr };
    drmModeModeInfo m_info {};
};

#endif // SRMCONNECTORMODE_H
