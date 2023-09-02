### Examples

To learn the basics, refer to the **[srm-basic](https://github.com/CuarzoSoftware/SRM/tree/main/src/examples/srm-basic)** example. Additionally, for creating textures from a single allocation and setting the hardware cursor pixels and position, you can check the **[srm-all-connectors](https://github.com/CuarzoSoftware/SRM/tree/main/src/examples/srm-all-connectors)** example.

### Running Included Examples

Those examples are included when installing SRM. To run them, follow these steps:

1. **srm-basic:** This example displays a continuously changing solid color on the screen until you press `CTRL + C` to stop it. It's a simple demonstration of SRM's graphics pipeline.

2. **srm-all-connectors:** In this example, SRM renders a shared texture created from a single allocation to all available connectors and animates the hardware cursor for 10 secs or until `CTRL + C` is pressed.

3. **srm-display-info:** This example outputs the current machine's configuration in JSON format, providing a snapshot of display-related information.

To run any of these examples, switch to a free TTY by pressing `CTRL + ALT + F[1, 2, 3, ... 10]`, `fn + CTRL + ALT + F[1, 2, 3, ... 10]`, or using the `chvt N` command (replace `N` with the desired virtual terminal number). Then, simply execute the desired example's command.