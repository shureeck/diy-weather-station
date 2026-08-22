SET(SOURCES ui_comp_container1.c
    ui_comp.c
    ui_Screen1.c
    ui.c
    ui_comp_hook.c
    ui_helpers.c
    ui_img_home_png_png.c
    ui_img_home_png.c
    ui_img_intemp_png.c
    ui_img_dropin_png.c
    ui_img_out_bg_png_png.c
    ui_img_tree_png.c
    ui_img_outtemp_png.c
    ui_img_humout_png.c
    ui_img_wifi_png.c
    fonts/ui_font_Font1.c)

add_library(ui ${SOURCES})
