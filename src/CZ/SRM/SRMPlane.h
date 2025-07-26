#ifndef SRMPLANE_H
#define SRMPLANE_H

#include <CZ/Ream/DRM/RDRMFormat.h>
#include <CZ/Ream/Ream.h>
#include <CZ/SRM/SRMObject.h>
#include <CZ/CZWeak.h>
#include <algorithm>
#include <string_view>
#include <unordered_set>
#include <array>
#include <xf86drmMode.h>

/**
 * @brief DRM plane associated with a DRM device.
 *
 * The @ref SRMPlane module provides functions for querying information about planes associated with DRM devices.
 * These planes can be used for various purposes, such as displaying content on connectors.
 *
 * @note This module is primarily used by SRM internally and may not be of much use to users.
 */
class CZ::SRMPlane final : public SRMObject
{
public:
    enum Type : UInt8
    {
        Overlay,
        Primary,
        Cursor
    };

    static const std::string_view &TypeString(Type type) noexcept
    {
        static constexpr std::array<std::string_view, 4> types { "Overlay", "Primary", "Cursor", "Unknown" };
        return types[std::clamp(type, Overlay, static_cast<Type>(Cursor + 1))];
    }

    /**
     * @brief Get the unique identifier of a DRM plane.
     *
     * @param plane A pointer to the @ref SRMPlane instance.
     *
     * @return The unique identifier of the DRM plane.
     */
    UInt32 id() const noexcept { return m_id; }

    /**
     * @brief Get the type of a DRM plane.
     *
     * @param plane A pointer to the @ref SRMPlane instance.
     *
     * @return The type of the DRM plane, as an @ref SRM_PLANE_TYPE enumeration value.
     */
    Type type() const noexcept { return m_type; }

    /**
     * @brief Get the DRM device to which a DRM plane belongs.
     *
     * @param plane A pointer to the @ref SRMPlane instance.
     *
     * @return A pointer to the @ref SRMDevice representing the DRM device.
     */
    SRMDevice *device() const noexcept { return m_device; }

    /**
     * @brief Get the current connector associated with a DRM plane.
     *
     * @param plane A pointer to the @ref SRMPlane instance.
     *
     * @return A pointer to the @ref SRMConnector representing the current connector, or `NULL` if not associated with any.
     */
    SRMConnector *currentConnector() const noexcept { return m_currentConnector; }

    /**
     * @brief Get a list of CRTCs associated with a DRM plane.
     *
     * @param plane A pointer to the @ref SRMPlane instance.
     *
     * @return A list of @ref SRMCrtc instances representing the CRTCs associated with the plane.
     */
    const std::vector<SRMCrtc*> &crtcs() const noexcept { return m_crtcs; }

    /**
     * @brief Get a list of DRM framebuffer formats supported by the plane.
     * *
     * @param plane A pointer to the @ref SRMPlane instance.
     *
     * @return A list of DRM formats/modifiers supported by the plane.
     */
    const RDRMFormatSet &formats() const noexcept { return m_formats; }
private:
    friend class SRMDevice;
    friend class SRMConnector;
    friend class SRMRenderer;

    static SRMPlane *Make(UInt32 id, SRMDevice *device) noexcept;
    SRMPlane(UInt32 id, SRMDevice *device) noexcept :
        m_id(id),
        m_device(device)
    {}
    bool initPropIds() noexcept;
    void initInFormats(UInt64 blobId) noexcept;
    bool initLegacyFormats(drmModePlanePtr res) noexcept;
    bool initCrtcs(drmModePlanePtr res) noexcept;

    UInt32 m_id;
    SRMDevice *m_device { nullptr };
    CZWeak<SRMConnector> m_currentConnector;
    std::vector<SRMCrtc*> m_crtcs;
    RDRMFormatSet m_formats;
    Type m_type;

    struct PropIDs
    {
        UInt32
            FB_ID,
            FB_DAMAGE_CLIPS,
            IN_FORMATS,
            IN_FENCE_FD,
            CRTC_ID,
            CRTC_X,
            CRTC_Y,
            CRTC_W,
            CRTC_H,
            SRC_X,
            SRC_Y,
            SRC_W,
            SRC_H,
            rotation,
            type;
    } m_propIDs {};
};

#endif // SRMPLANE_H
