#ifndef SRMFORMAT_H
#define SRMFORMAT_H

#include "SRMTypes.h"
#include <GLES2/gl2.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup SRMFormat SRMFormat
 *
 * @brief Buffer formats and conversions.
 *
 * The SRMFormat module provides structures and functions for working with various buffer formats,
 * including conversions between DRM and OpenGL formats, format lists, and format-related operations.
 *
 * @{
 */

/**
 * @brief Structure representing a buffer format with a format code and modifier.
 */
typedef struct SRMFormatStruct
{
    UInt32 format;   /**< The DRM format code. */
    UInt64 modifier; /**< The DRM modifier associated with the format. */
} SRMFormat;

/**
 * @brief Structure representing a mapping between DRM and OpenGL buffer formats.
 */
typedef struct SRMGLFormatStruct
{
    UInt32 drmFormat;      /**< DRM buffer format code. */
    GLint glInternalFormat; /**< OpenGL internal texture format. */
    GLint glFormat;         /**< OpenGL texture format. */
    GLint glType;           /**< OpenGL texture pixel type. */
    UInt8 hasAlpha;         /**< Flag indicating whether the format has an alpha channel. */
} SRMGLFormat;

/**
 * @brief Convert a DRM buffer format to an equivalent OpenGL format.
 *
 * @param format The DRM buffer format code to convert.
 *
 * @return A pointer to the @ref SRMGLFormat structure representing the equivalent OpenGL format.
 */
const SRMGLFormat *srmFormatDRMToGL(SRM_BUFFER_FORMAT format);

/**
 * @brief Add a buffer format to a list of formats.
 *
 * @param formatsList A list of formats to add to.
 * @param format The buffer format code to add.
 * @param modifier The modifier associated with the format.
 *
 * @return A pointer to the newly added @ref SRMFormat structure.
 */
SRMListItem *srmFormatsListAddFormat(SRMList *formatsList, UInt32 format, UInt64 modifier);

/**
 * @brief Check if a format/modifier pair is in the list.
 *
 * @param format The buffer format code.
 * @param modifier The modifier associated with the format.
 *
 * @return 1 if in the list, 0 otherwise.
 */
UInt8 srmFormatIsInList(SRMList *formatsList, UInt32 format, UInt64 modifier);

/**
 * @brief Find the first format matching the specified format code in a list of formats.
 *
 * @param formatsList A list of formats to search in.
 * @param format The buffer format code to match.
 *
 * @return A pointer to the first @ref SRMFormat structure matching the format code, or `NULL` if not found.
 */
SRMFormat *srmFormatListFirstMatchFormat(SRMList *formatsList, UInt32 format);

/**
 * @brief Get the depth and bits per pixel (BPP) of a buffer format.
 *
 * @param format The buffer format code.
 * @param depth A pointer to store the depth of the format.
 * @param bpp A pointer to store the bits per pixel (BPP) of the format.
 *
 * @return 1 if the depth and BPP were successfully retrieved, 0 if the format is not recognized.
 */
UInt8 srmFormatGetDepthBpp(SRM_BUFFER_FORMAT format, UInt32 *depth, UInt32 *bpp);

/**
 * @brief Create a copy of a list of formats.
 *
 * @param formatsList A list of formats to copy.
 *
 * @return A new list containing a copy of the input formats.
 */
SRMList *srmFormatsListCopy(SRMList *formatsList);

/**
 * @brief Destroy a list of formats and free associated resources.
 *
 * @param formatsList A pointer to the list of formats to destroy. The pointer is set to `NULL` after destruction.
 */
void srmFormatsListDestroy(SRMList **formatsList);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif // SRMFORMAT_H
