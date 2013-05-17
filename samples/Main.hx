package;

import ogl.GL;
import ogl.GLM;
import glfw3.GLFW;
import gl3font.Font;

class Main {
    static function main() {
        GLFW.init();
        var window = GLFW.createWindow(800, 600, "Test GL3Font");
        GLFW.makeContextCurrent(window);
        GL.init();

        GL.viewport(0,0,800,600);
        GL.enable(GL.BLEND);
        GL.blendFunc(GL.SRC_ALPHA, GL.ONE_MINUS_SRC_ALPHA);

        var face = "../free/sans";
        var font = new Font(face+".json", face+".png");

        var buf = new StringBuffer(font);
        var bounds = buf.set("_,;:'\"Hello World!^*%#.");

        var renderer = new FontRenderer();
        while (!GLFW.windowShouldClose(window)) {
            GLFW.pollEvents();

            var time = GLFW.getTime();
            var rotation = time;
            var size = 80 + Math.sin(time)*50;
            var pos = GLFW.getCursorPos(window);
            var x = pos.x;
            var y = pos.y;

            GL.clear(GL.COLOR_BUFFER_BIT);

            renderer.begin();
            renderer.setColour([1,1,1,1]);
            renderer.setTransform(
                Mat3x2.viewportMap(800, 600) *
                Mat3x2.translate(x, y) *
                Mat3x2.scale(size/font.baseSize, size/font.baseSize) *
                Mat3x2.rotate(rotation) *
                Mat3x2.translate(-bounds.x-bounds.w/2, font.baseSize/2)
            );
            renderer.render(buf);
            renderer.end();

            GLFW.swapBuffers(window);
        }
    }
}

