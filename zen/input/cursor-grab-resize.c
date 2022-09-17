#include "zen/input/cursor-grab-resize.h"

#include "zen-common.h"

static void zn_cursor_grab_resize_end(struct zn_cursor_grab_resize* self);

static void
resize_grab_motion(
    struct zn_cursor_grab* grab, struct wlr_event_pointer_motion* event)
{
  UNUSED(event);
  struct zn_cursor_grab_resize* self = zn_container_of(grab, self, base);

  zn_cursor_move_relative(grab->cursor, event->delta_x, event->delta_y);

  if (self->view->board->screen != grab->cursor->screen) {
    return;
  }

  if (self->view->resize_status.acked) {
    return;
  }

  struct wlr_box view_geometry;
  self->view->impl->get_geometry(self->view, &view_geometry);
  const double width = grab->cursor->x - self->init_cursor_x;
  const double height = grab->cursor->y - self->init_cursor_y;

  // alias
  struct wlr_fbox* box = &self->view->resize_status.requested_box;
  *box = (struct wlr_fbox){
      .x = self->init_view_box.x,
      .y = self->init_view_box.y,
      .width = self->init_view_box.width,
      .height = self->init_view_box.height,
  };

  if (self->edges & WLR_EDGE_LEFT) {
    box->x = grab->cursor->x - view_geometry.x - self->diff_x;
    box->width -= width;
  }
  if (self->edges & WLR_EDGE_RIGHT) {
    box->width += width;
  }
  if (self->edges & WLR_EDGE_TOP) {
    box->y = grab->cursor->y - view_geometry.y - self->diff_y;
    box->height -= height;
  }
  if (self->edges & WLR_EDGE_BOTTOM) {
    box->height += height;
  }

  self->view->resize_status.serial =
      self->view->impl->set_size(self->view, box->width, box->height);
}

static void
resize_grab_button(
    struct zn_cursor_grab* grab, struct wlr_event_pointer_button* event)
{
  struct zn_cursor_grab_resize* self = zn_container_of(grab, self, base);

  if (event->state == WLR_BUTTON_RELEASED) {
    zn_cursor_grab_resize_end(self);
  }
}

static void
resize_grab_axis(
    struct zn_cursor_grab* grab, struct wlr_event_pointer_axis* event)
{
  UNUSED(grab);
  UNUSED(event);
}

static void
resize_grab_frame(struct zn_cursor_grab* grab)
{
  UNUSED(grab);
}

static void
resize_grab_rebase(struct zn_cursor_grab* grab)
{
  struct zn_cursor_grab_resize* self = zn_container_of(grab, self, base);
  struct zn_board* board = zn_screen_get_current_board(grab->cursor->screen);
  zn_view_move(self->view, board, grab->cursor->x, grab->cursor->y);
}

static void
resize_grab_cancel(struct zn_cursor_grab* grab)
{
  struct zn_cursor_grab_resize* self = zn_container_of(grab, self, base);
  // zn_view_set_size(self->view, &self->init_view_box);
  zn_cursor_grab_resize_end(self);
}

static const struct zn_cursor_grab_interface resize_grab_interface = {
    .motion = resize_grab_motion,
    .button = resize_grab_button,
    .axis = resize_grab_axis,
    .frame = resize_grab_frame,
    .rebase = resize_grab_rebase,
    .cancel = resize_grab_cancel,
};

static void
zn_cursor_grab_resize_handle_view_unmap(
    struct wl_listener* listener, void* data)
{
  UNUSED(data);
  struct zn_cursor_grab_resize* self =
      zn_container_of(listener, self, view_unmap_listener);
  zn_cursor_grab_resize_end(self);
}

static struct zn_cursor_grab_resize*
zn_cursor_grab_resize_create(
    struct zn_cursor* cursor, struct zn_view* view, uint32_t edges)
{
  struct zn_cursor_grab_resize* self;
  self = zalloc(sizeof *self);

  if (self == NULL) {
    zn_error("Failed to allocate memory");
    return NULL;
  }

  zn_view_get_window_fbox(view, &self->init_view_box);

  self->diff_x = cursor->x - self->init_view_box.x;
  self->diff_y = cursor->y - self->init_view_box.y;

  self->init_cursor_x = cursor->x;
  self->init_cursor_y = cursor->y;

  self->init_view_box.x = view->x;
  self->init_view_box.y = view->y;

  self->view = view;
  self->edges = edges;
  self->base.interface = &resize_grab_interface;
  self->base.cursor = cursor;

  view->resize_status.edges = edges;

  self->view_unmap_listener.notify = zn_cursor_grab_resize_handle_view_unmap;
  wl_signal_add(&view->events.unmap, &self->view_unmap_listener);

  return self;
}

static void
zn_cursor_grab_resize_destroy(struct zn_cursor_grab_resize* self)
{
  wl_list_remove(&self->view_unmap_listener.link);
  free(self);
}

static void
zn_cursor_grab_resize_end(struct zn_cursor_grab_resize* self)
{
  self->view->resize_status.resizing = false;
  zn_cursor_end_grab(self->base.cursor);
  zn_cursor_grab_resize_destroy(self);
}

void
zn_cursor_grab_resize_start(
    struct zn_cursor* cursor, struct zn_view* view, uint32_t edges)
{
  struct zn_server* server = zn_server_get_singleton();
  struct wlr_seat* seat = server->input_manager->seat->wlr_seat;

  struct zn_cursor_grab_resize* self =
      zn_cursor_grab_resize_create(cursor, view, edges);
  if (!self) {
    return;
  }

  const char* xcursor_name[] = {
      [WLR_EDGE_TOP] = "n-resize",
      [WLR_EDGE_BOTTOM] = "s-resize",
      [WLR_EDGE_LEFT] = "w-resize",
      [WLR_EDGE_RIGHT] = "e-resize",
      [WLR_EDGE_TOP | WLR_EDGE_LEFT] = "nw-resize",
      [WLR_EDGE_TOP | WLR_EDGE_RIGHT] = "ne-resize",
      [WLR_EDGE_BOTTOM | WLR_EDGE_LEFT] = "sw-resize",
      [WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT] = "se-resize",
  };

  view->resize_status.resizing = true;
  wlr_seat_pointer_clear_focus(seat);
  zn_cursor_set_xcursor(cursor, xcursor_name[edges]);
  zn_cursor_start_grab(cursor, &self->base);
}