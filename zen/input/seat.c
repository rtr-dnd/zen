#include "zen/input/seat.h"

#include <wlr/types/wlr_seat.h>
#include <zigen-protocol.h>

#include "zen-common.h"
#include "zen/input/input-device.h"

static void
zn_seat_update_capabilities(struct zn_seat *self)
{
  uint32_t wl_caps = 0;
  uint32_t zgn_caps = 0;

  struct zn_input_device *input_device;
  wl_list_for_each (input_device, &self->devices, link) {
    switch (input_device->wlr_input->type) {
      case WLR_INPUT_DEVICE_KEYBOARD:
        wl_caps |= WL_SEAT_CAPABILITY_KEYBOARD;
        break;
      case WLR_INPUT_DEVICE_POINTER:
        wl_caps |= WL_SEAT_CAPABILITY_POINTER;
        zgn_caps |= ZGN_SEAT_CAPABILITY_RAY_DIRECTION;
        break;
      case WLR_INPUT_DEVICE_TOUCH:
        // TODO: support touch device
        break;
      case WLR_INPUT_DEVICE_TABLET_TOOL:
      case WLR_INPUT_DEVICE_TABLET_PAD:
      case WLR_INPUT_DEVICE_SWITCH:
        break;
    }
  }

  wlr_seat_set_capabilities(self->wlr_seat, wl_caps);
  zgnr_seat_set_capabilities(self->zgnr_seat, zgn_caps);
}

void
zn_seat_add_device(struct zn_seat *self, struct zn_input_device *input_device)
{
  wl_list_insert(&self->devices, &input_device->link);
  zn_seat_update_capabilities(self);
}

void
zn_seat_remove_device(
    struct zn_seat *self, struct zn_input_device *input_device)
{
  wl_list_remove(&input_device->link);
  zn_seat_update_capabilities(self);
}

struct zn_seat *
zn_seat_create(struct wl_display *display, const char *seat_name)
{
  struct zn_seat *self;
  self = zalloc(sizeof *self);
  if (self == NULL) {
    zn_error("Failed to allocate memory");
    goto err;
  }

  self->wlr_seat = wlr_seat_create(display, seat_name);
  if (self->wlr_seat == NULL) {
    zn_error("Failed to create wlr_seat");
    goto err_free;
  }

  self->zgnr_seat = zgnr_seat_create(display);
  if (self->zgnr_seat == NULL) {
    zn_error("Failed to creat zgnr_seat");
    goto err_wlr_seat;
  }

  wl_list_init(&self->devices);
  wl_signal_init(&self->events.destroy);

  return self;

err_wlr_seat:
  wlr_seat_destroy(self->wlr_seat);

err_free:
  free(self);

err:
  return NULL;
}

void
zn_seat_destroy(struct zn_seat *self)
{
  wl_signal_emit(&self->events.destroy, NULL);

  wl_list_remove(&self->devices);
  zgnr_seat_destroy(self->zgnr_seat);
  wlr_seat_destroy(self->wlr_seat);
  free(self);
}