// Ionic web components in iOS mode (docs/ARCHITECTURE.md, UI chrome), without
// Angular, React or Vue: the custom elements build of @ionic/core, with
// `initialize()` for the config and each component registered by its
// `defineCustomElement` (@ionic/core README, "Custom Elements Build";
// ionicframework.com/docs/intro/cdn).
//
// JSX uses the Solid components of @ionic-solidjs/core (github.com/
// ionic-solidjs/ionic-solidjs), not <ion-*> tags: each one creates its element
// alone and inserts the children afterwards. A Solid template holding nested
// Ionic tags upgrades them while it is cloned, and Stencil's slot patches on
// the scoped components (ion-buttons, ion-header, ion-label, ...) then hide
// their children from the template walk (ionic-framework#29756).
//
// Overlays are created with the overlay
// controllers, whose `component` may be an element (README, "Using Overlay
// Controllers"); Solid renders the overlay's content into that element.
import {
  actionSheetController,
  alertController,
  initialize,
  modalController,
  popoverController,
  toastController,
  type ActionSheetButton,
  type AlertInput,
  type ModalOptions,
  type PopoverOptions,
} from "@ionic/core/components";
import { defineCustomElement as defineActionSheet } from "@ionic/core/components/ion-action-sheet.js";
import { defineCustomElement as defineAlert } from "@ionic/core/components/ion-alert.js";
import { defineCustomElement as defineModal } from "@ionic/core/components/ion-modal.js";
import { defineCustomElement as definePopover } from "@ionic/core/components/ion-popover.js";
import { defineCustomElement as defineToast } from "@ionic/core/components/ion-toast.js";
import type { JSX } from "solid-js";
import { render } from "solid-js/web";

import "@ionic/core/css/core.css";
import "@ionic/core/css/normalize.css";
import "@ionic/core/css/structure.css";
import "@ionic/core/css/typography.css";

// iOS mode on every platform: the web chrome matches the iPad host's SwiftUI
// controls (docs/specs/tablet-ui.md, Visual style).
initialize({ mode: "ios" });
// The components in JSX come from @ionic-solidjs/core, each registered when its
// Solid component is imported. The overlays below are created by controllers,
// not in JSX, so they are registered here.
for (const define of [defineActionSheet, defineAlert, defineModal, definePopover, defineToast]) define();

// A control placed from the mockups whose feature has not landed: using it
// says so and names the issue that implements it (#57).
export function notImplemented(issue: number): void {
  void toast(`Not implemented yet (#${issue})`);
}

export async function toast(message: string, color?: "danger"): Promise<void> {
  const t = await toastController.create({ message, color, duration: 2500, position: "bottom" });
  await t.present();
}

// Closes the overlay; resolves once it is gone.
type Dismiss = () => Promise<void>;

// An element holding `view`, disposed when the overlay is gone.
function mount(view: (dismiss: Dismiss) => JSX.Element, dismiss: Dismiss, className: string) {
  const host = document.createElement("div");
  host.className = className;
  const dispose = render(() => view(dismiss), host);
  return { host, dispose };
}

// A popover anchored at the control `event` came from (a menu, the pen editor).
export async function presentPopover(
  event: Event,
  view: (dismiss: Dismiss) => JSX.Element,
  options: Omit<PopoverOptions, "component" | "event"> = {},
): Promise<void> {
  let popover: HTMLIonPopoverElement | undefined;
  const { host, dispose } = mount(view, async () => void (await popover?.dismiss()), "popover-body");
  popover = await popoverController.create({ component: host, event, ...options });
  void popover.onDidDismiss().then(dispose);
  await popover.present();
}

// A modal sheet (New Notebook, New Note).
export async function presentModal(
  view: (dismiss: Dismiss) => JSX.Element,
  options: Omit<ModalOptions, "component"> = {},
): Promise<void> {
  let modal: HTMLIonModalElement | undefined;
  const { host, dispose } = mount(view, async () => void (await modal?.dismiss()), "ion-page");
  modal = await modalController.create({ component: host, ...options });
  void modal.onDidDismiss().then(dispose);
  await modal.present();
}

// An alert with one text field; `accept` returns an error to show, or null
// when the value is taken and the alert closes.
export async function promptText(options: {
  header: string;
  label: string;
  value?: string;
  action: string;
  accept: (value: string) => string | null;
}): Promise<void> {
  const input: AlertInput = { name: "text", type: "text", value: options.value ?? "", attributes: { "aria-label": options.label } };
  const alert = await alertController.create({
    header: options.header,
    inputs: [input],
    buttons: [
      { text: "Cancel", role: "cancel" },
      {
        text: options.action,
        handler: (values: { text: string }) => {
          const error = options.accept(values.text);
          if (error === null) return true;
          alert.message = error;
          return false;
        },
      },
    ],
  });
  await alert.present();
}

// An action sheet of choices, with Cancel.
export async function chooseAction(header: string, buttons: ActionSheetButton[]): Promise<void> {
  const sheet = await actionSheetController.create({ header, buttons: [...buttons, { text: "Cancel", role: "cancel" }] });
  await sheet.present();
}
