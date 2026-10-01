import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    objectName: "projectDialog"

    property bool creating: false

    title: creating ? qsTr("New Project") : qsTr("Project Settings")
    modal: true
    standardButtons: Dialog.Ok | Dialog.Cancel
    width: Math.min(parent ? parent.width - 48 : 660, 660)
    height: Math.min(parent ? parent.height - 48 : 720, 720)

    function plannedControls() {
        return [
            { targetId: "v9938", control: v9938Target },
            { targetId: "v9958", control: v9958Target },
            { targetId: "sega-sms-vdp", control: segaSmsTarget },
            { targetId: "sega-genesis-vdp", control: segaGenesisTarget },
            { targetId: "huc6270", control: huc6270Target },
            { targetId: "vic-ii", control: vicIiTarget },
            { targetId: "vic", control: vicTarget },
            { targetId: "game-boy-ppu", control: gameBoyTarget },
            { targetId: "game-boy-color-ppu", control: gameBoyColorTarget },
            { targetId: "super-nes-ppu", control: superNesTarget },
            { targetId: "atari-lynx", control: atariLynxTarget },
            { targetId: "neo-geo", control: neoGeoTarget },
            { targetId: "amstrad-cpc", control: amstradCpcTarget },
            { targetId: "ibm-cga", control: ibmCgaTarget },
            { targetId: "ibm-ega", control: ibmEgaTarget },
            { targetId: "apple-iie", control: appleIieTarget },
            { targetId: "timex-ts1000", control: timexTarget },
            { targetId: "trs80-model1-3", control: trs80Target }
        ]
    }

    function setPlannedTargets(targetIds) {
        const controls = plannedControls()
        for (let index = 0; index < controls.length; ++index)
            controls[index].control.checked = targetIds.indexOf(
                                                  controls[index].targetId) >= 0
    }

    function selectedPlannedTargets() {
        const selected = []
        const controls = plannedControls()
        for (let index = 0; index < controls.length; ++index) {
            if (controls[index].control.checked)
                selected.push(controls[index].targetId)
        }
        return selected
    }

    function openForNewProject() {
        creating = true
        projectNameField.text = qsTr("Untitled Project")
        tms9918aTarget.checked = true
        f18aTarget.checked = true
        setPlannedTargets([])
        open()
        projectNameField.forceActiveFocus()
        projectNameField.selectAll()
    }

    function openForProjectSettings() {
        creating = false
        projectNameField.text = editorProject.projectName
        tms9918aTarget.checked = editorProject.tms9918aEnabled
        f18aTarget.checked = editorProject.f18aEnabled
        setPlannedTargets(editorProject.plannedTargetIds)
        open()
        projectNameField.forceActiveFocus()
    }

    onAccepted: {
        const plannedTargets = selectedPlannedTargets()
        if (creating) {
            editorProject.createProjectWithTargets(projectNameField.text,
                                                   tms9918aTarget.checked,
                                                   f18aTarget.checked,
                                                   plannedTargets)
        } else {
            editorProject.configureProjectWithTargets(projectNameField.text,
                                                      tms9918aTarget.checked,
                                                      f18aTarget.checked,
                                                      plannedTargets)
        }
    }

    ScrollView {
        id: targetScroll
        anchors.fill: parent
        clip: true

        ColumnLayout {
            width: targetScroll.availableWidth
            spacing: 6

            Label { text: qsTr("Project name") }
            TextField {
                id: projectNameField
                objectName: "projectNameField"
                Layout.fillWidth: true
                placeholderText: qsTr("Untitled Project")
                Accessible.name: qsTr("Project name")
            }

            Label {
                Layout.topMargin: 10
                text: qsTr("Project targets")
                font.pixelSize: 17
                font.weight: Font.DemiBold
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: palette.placeholderText
                text: qsTr("Implemented targets are available in the Project Bar. Future targets are recorded in the project now and become active as their rule engines are implemented.")
            }

            Label {
                Layout.topMargin: 8
                text: qsTr("True video display processors (VDP)")
                font.weight: Font.DemiBold
            }
            CheckBox {
                id: tms9918aTarget
                objectName: "tms9918aProjectTarget"
                text: qsTr("TMS9918A — Implemented")
                enabled: !checked || f18aTarget.checked
            }
            CheckBox {
                id: f18aTarget
                objectName: "f18aProjectTarget"
                text: qsTr("F18A — Implemented")
                enabled: !checked || tms9918aTarget.checked
            }
            CheckBox {
                id: v9938Target
                objectName: "v9938ProjectTarget"
                text: qsTr("Yamaha V9938 — Next")
            }
            CheckBox {
                id: v9958Target
                objectName: "v9958ProjectTarget"
                text: qsTr("Yamaha V9958 — Planned")
            }
            CheckBox {
                id: segaSmsTarget
                objectName: "segaSmsProjectTarget"
                text: qsTr("Sega Master System 315-5124 / 315-5246 — Planned")
            }
            CheckBox {
                id: segaGenesisTarget
                objectName: "segaGenesisProjectTarget"
                text: qsTr("Sega Genesis / Mega Drive 315-5313 / YM7101 — Planned")
            }
            CheckBox {
                id: huc6270Target
                objectName: "huc6270ProjectTarget"
                text: qsTr("NEC / Hudson HuC6270 — Planned")
            }
            CheckBox {
                id: vicIiTarget
                objectName: "vicIiProjectTarget"
                text: qsTr("MOS VIC-II — Planned")
            }
            CheckBox {
                id: vicTarget
                objectName: "vicProjectTarget"
                text: qsTr("MOS VIC — Planned")
            }

            Label {
                Layout.topMargin: 8
                text: qsTr("Picture processors (PPU)")
                font.weight: Font.DemiBold
            }
            CheckBox {
                id: gameBoyTarget
                objectName: "gameBoyProjectTarget"
                text: qsTr("Nintendo Game Boy — Planned")
            }
            CheckBox {
                id: gameBoyColorTarget
                objectName: "gameBoyColorProjectTarget"
                text: qsTr("Nintendo Game Boy Color — Planned")
            }
            CheckBox {
                id: superNesTarget
                objectName: "superNesProjectTarget"
                text: qsTr("Super NES 5C77 / 5C78 — Planned")
            }

            Label {
                Layout.topMargin: 8
                text: qsTr("Object processors")
                font.weight: Font.DemiBold
            }
            CheckBox {
                id: atariLynxTarget
                objectName: "atariLynxProjectTarget"
                text: qsTr("Atari Lynx Suzy / Mikey — Planned")
            }
            CheckBox {
                id: neoGeoTarget
                objectName: "neoGeoProjectTarget"
                text: qsTr("SNK Neo Geo — Research")
            }

            Label {
                Layout.topMargin: 8
                text: qsTr("Display adapters")
                font.weight: Font.DemiBold
            }
            CheckBox {
                id: amstradCpcTarget
                objectName: "amstradCpcProjectTarget"
                text: qsTr("Amstrad CPC — Planned")
            }
            CheckBox {
                id: ibmCgaTarget
                objectName: "ibmCgaProjectTarget"
                text: qsTr("IBM CGA — Research")
            }
            CheckBox {
                id: ibmEgaTarget
                objectName: "ibmEgaProjectTarget"
                text: qsTr("IBM EGA — Research")
            }
            CheckBox {
                id: appleIieTarget
                objectName: "appleIieProjectTarget"
                text: qsTr("Apple IIe — Research")
            }

            Label {
                Layout.topMargin: 8
                text: qsTr("Character-display hardware")
                font.weight: Font.DemiBold
            }
            CheckBox {
                id: timexTarget
                objectName: "timexTs1000ProjectTarget"
                text: qsTr("Timex Sinclair 1000 / ZX81-class — Research")
            }
            CheckBox {
                id: trs80Target
                objectName: "trs80ProjectTarget"
                text: qsTr("TRS-80 Model I / III — Research")
            }

            Label {
                Layout.fillWidth: true
                Layout.topMargin: 10
                Layout.bottomMargin: 8
                wrapMode: Text.WordWrap
                color: palette.placeholderText
                text: qsTr("The active target controls which creation modes and hardware rules are available. At least one implemented target is required.")
            }
        }
    }
}
