#include <SRMCore.h>
#include <SRMDevice.h>
#include <SRMListener.h>
#include <SRMCrtc.h>
#include <SRMEncoder.h>
#include <SRMPlane.h>
#include <SRMConnector.h>
#include <SRMConnectorMode.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

using namespace SRM;

static int openRestricted(const char *path, int flags, void *)
{
    // Here libseat could be used instead
    return open(path, flags);
}

static void closeRestricted(int fd, void *)
{
    close(fd);
}

SRMInterface srmInterface
{
    .openRestricted = &openRestricted,
    .closeRestricted = &closeRestricted
};

static void initializeGL(SRMConnector *connector, void *)
{

}

static void paintGL(SRMConnector *connector, void *)
{

}

static void resizeGL(SRMConnector *connector, void *)
{

}

static void uninitializeGL(SRMConnector *connector, void *)
{

}

void deviceCreatedEvent(SRMListener *, SRMDevice *device)
{
    fprintf(stdout, "DRM device created: %s\n", device->name());
}

void deviceRemovedEvent(SRMListener *, SRMDevice *device)
{
    fprintf(stdout, "DRM device removed: %s\n", device->name());
}

void connectorPluggedEvent(SRMListener *, SRMConnector *connector)
{
    fprintf(stdout, "DRM connector plugged: %s\n", "HOLA");
}

void connectorUnpluggedEvent(SRMListener *, SRMConnector *connector)
{
    fprintf(stdout, "DRM connector unplugged: %s\n", "HOLA");
}

int main(void)
{
    SRMCore *srm = SRMCore::createSRM(&srmInterface);

    if (!srm)
    {
        fprintf(stderr, "Failed to initialize SRM.\n");
        return 1;
    }

    // Subscribe to DRM events
    SRMListener *deviceCreatedListener = srm->addDeviceCreatedListener(&deviceCreatedEvent);
    SRMListener *connectorPluggedListener = srm->addConnectorPluggedListener(&connectorPluggedEvent);

    printf("Allocator Device: %s\n", srm->allocatorDevice()->name());

    // Print devices information
    for (SRMDevice *device : srm->devices())
    {
        printf("Device: %s\n", device->name());

        printf("Client Caps:\n");
        printf("- STEREO 3D: %s\n",            device->clientCapStereo3D()             ? "YES" : "NO");
        printf("- UNIVERSAL PLANES: %s\n",     device->clientCapUniversalPlanes()      ? "YES" : "NO");
        printf("- ATOMIC: %s\n",               device->clientCapAtomic()               ? "YES" : "NO");
        printf("- ASPECT RATIO: %s\n",         device->clientCapAspectRatio()          ? "YES" : "NO");
        printf("- WRITEBACK CONNECTORS: %s\n", device->clientCapWritebackConnectors()  ? "YES" : "NO");

        printf("Caps:\n");
        printf("- DUMB BUFFER: %s\n",          device->capDumbBuffer()                 ? "YES" : "NO");
        printf("- PRIME IMPORT: %s\n",         device->capPrimeImport()                ? "YES" : "NO");
        printf("- PRIME EXPORT: %s\n",         device->capPrimeExport()                ? "YES" : "NO");
        printf("- FB2 MODIFIERS: %s\n",        device->capAddFb2Modifiers()            ? "YES" : "NO");

        printf("Render:\n");
        printf("- IS RENDERER: %s\n",          device->isRenderer()                     ? "YES" : "NO");
        printf("- RENDERER DEVICE: %s\n",      device->rendererDevice()->name());
        printf("- RENDER MODE: %s\n",          getRenderModeString(device->renderMode()));

        printf("Crtcs:\n");

        for (SRMCrtc *crtc : device->crtcs())
            printf("- Crtc (%d)\n", crtc->id());

        printf("Encoders:\n");

        for (SRMEncoder *encoder : device->encoders())
        {
            printf("- Encoder (%d): [", encoder->id());

            for (SRMCrtc *crtc : encoder->crtcs())
            {
                if (crtc == encoder->crtcs().back())
                    printf("%d]\n", crtc->id());
                else
                    printf("%d, ", crtc->id());
            }
        }

        printf("Planes:\n");

        for (SRMPlane *plane : device->planes())
        {
            printf("- Plane (%d):\n\t- Type: %s\n\t- Crtcs: [", plane->id(), getPlaneTypeString(plane->type()));

            for (SRMCrtc *crtc : plane->crtcs())
            {
                if (crtc == plane->crtcs().back())
                    printf("%d]\n", crtc->id());
                else
                    printf("%d, ", crtc->id());
            }
        }

        printf("Connectors:\n");

        for (SRMConnector *connector : device->connectors())
        {
            printf("- Connector (%d):\n", connector->id());
            printf("\tConnected: %s\n", connector->connected() ? "YES" : "NO");
            printf("\tStatus: %s\n", getConnectorStateString(connector->state()));
            printf("\tName: %s\n", connector->name());
            printf("\tModel: %s\n", connector->model());
            printf("\tManufacturer: %s\n", connector->manufacturer());
            printf("\tPhysical Size: %dx%d mm\n", connector->mmWidth(), connector->mmHeight());

            printf("\tEncoders: [");

            for (SRMEncoder *encoder : connector->encoders())
            {
                if (encoder == connector->encoders().back())
                    printf("%d]\n", encoder->id());
                else
                    printf("%d, ", encoder->id());
            }

            printf("\tModes:\n");

            for (SRMConnectorMode *connectorMode : connector->modes())
            {
                printf("\t - %dx%d@%dHz\n", connectorMode->width(), connectorMode->height(), connectorMode->refreshRate());
            }
        }

    }

    while (1)
    {
        // Poll DRM devices/connectors hotplugging events (0 disables timeout)
        if (srm->processMonitor(-1) < 0)
            break;
    }

    // Unsuscribe to DRM events
    delete deviceCreatedListener;
    delete connectorPluggedListener;

    return 0;
}
