// veikk driver device-independent code
// TODO: standardize comments

#include <linux/module.h>
#include "veikk.h"

// TODO: add spinlock for this
// this stores all connected devices (is a circ. dll of struct veikk)
LIST_HEAD(vdevs);

// veikk_input_open/close are used for the input_dev open/close events, never
// called directly
int veikk_input_open(struct input_dev *dev) {
    return hid_hw_open(((struct veikk *) input_get_drvdata(dev))->hdev);
}
void veikk_input_close(struct input_dev *dev) {
    hid_hw_close(((struct veikk *) input_get_drvdata(dev))->hdev);
}

static int veikk_probe(struct hid_device *hdev,
                       const struct hid_device_id *id) {
    struct usb_interface *intf = to_usb_interface(hdev->dev.parent);
    struct usb_device *dev = interface_to_usbdev(intf);
    struct veikk *veikk;
    int error;

    if(!id->driver_data)
        return -EINVAL;

    // alloc kmem for struct veikk associated with driver
    if(!(veikk = devm_kzalloc(&hdev->dev, sizeof(struct veikk), GFP_KERNEL)))
        return -ENOMEM;
    hid_set_drvdata(hdev, veikk);
    veikk->hdev = hdev;
    veikk->vdinfo = (struct veikk_device_info *) id->driver_data;

    // initialize veikk mapping defaults from vdinfo and calculations from
    // veikk_screen_map and veikk_screen_size module parameters
    // TODO: put under spinlock
    // TODO: document concurrency issues with modparms
    veikk_configure_input_devs(veikk_screen_map, veikk_screen_size,
                               veikk_orientation, veikk);

    // TODO: set quirks? probably not necessary
    // TODO: set up workqueue interface? doesn't seem necessary
    // TODO: allocate pen fifo? doesn't seem necessary
    // TODO: does this part have to done under a mutex? doesn't seem necessary

    // load/parse report descriptor
    if((error = hid_parse(hdev)))
        return error;

    if((error = (*veikk->vdinfo->alloc_input_devs)(veikk))) {
        hid_err(hdev, "alloc_input_devs failed\n");
        goto fail;
    }

    if((error = (*veikk->vdinfo->setup_and_register_input_devs)(veikk))) {
        hid_err(hdev, "setup_and_register_input_devs failed\n");
        goto fail;
    }

    if((error = hid_hw_start(hdev, HID_CONNECT_HIDRAW|HID_CONNECT_DRIVER))) {
        hid_err(hdev, "hw start failed\n");
        goto fail;
    }

    // add to vdevs
    // TODO: do under spinlock
    list_add(&veikk->lh, &vdevs);

    hid_info(veikk->hdev, "%s probed successfully.\n", veikk->vdinfo->name);
    return 0;

fail:
    // TODO: release resources
    return error;
}

static void veikk_remove(struct hid_device *hdev) {
    struct veikk *veikk = hid_get_drvdata(hdev);

    hid_hw_close(hdev);
    hid_hw_stop(hdev);

    // TODO: clean up workqueues (if applicable)
    // TODO: free resources (e.g., pen_fifo) if applicable

    // TODO: remove drvdata from hid

    // remove from vdevs
    // TODO: do under spinlock
    list_del(&veikk->lh);

    hid_info(veikk->hdev, "%s removed.\n", veikk->vdinfo->name);
}

static int veikk_raw_event(struct hid_device *hdev, struct hid_report *report,
                           u8 *data, int size) {
    struct veikk *veikk = hid_get_drvdata(hdev);

    // call device-specific raw input report handler
    return (*veikk->vdinfo->handle_raw_data)(veikk, data, size);
}

// uncomment for testing: read input reports
//void veikk_report(struct hid_device *hdev, struct hid_report *report) {
//    int i, j;
//    struct hid_field *field;
//    struct hid_usage *usage;
//
//    hid_info(hdev, "parsing report: %d %d %d %d %d\n",
//             report->id, report->type, report->application, report->maxfield,
//             report->size);
//
//    for(i=0; i<report->maxfield; i++) {
//        for(j=0; j<report->field[i]->maxusage; j++) {
//            field = report->field[i];
//            usage = &field->usage[j];
//
//            // TODO: read/print actual data at these offsets
//            hid_info(hdev, "field %d usage %d: %x; offset: %d; size: %d\n",
//                           i, j, usage->hid, field->report_offset,
//                           field->report_size);
//        }
//    }
//}

/* Register module */
static struct hid_driver veikk_driver = {
    .name = "veikk",
    .id_table = veikk_ids,
    .probe = veikk_probe,
    .remove = veikk_remove,
    .raw_event = veikk_raw_event,
//    .report = veikk_report    // uncomment for testing
};
module_hid_driver(veikk_driver);

// TODO: add other metadata (MODULE_VERSION, MODULE_AUTHOR, MODULE_DESCRIPTION,
MODULE_VERSION(VEIKK_DRIVER_VERSION);
MODULE_AUTHOR(VEIKK_DRIVER_AUTHOR);
MODULE_DESCRIPTION(VEIKK_DRIVER_DESC);
MODULE_LICENSE("GPL");
