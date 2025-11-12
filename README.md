# ExfilShield

**ExfilShield** is a Windows service written in C++ that monitors and controls the connection of external devices — USB, MTP, HID, network adapters, etc. — to prevent unauthorized data exfiltration or device misuse.

This is a **personal project** created to gain experience in developing **C++ Windows endpoint security applications and services**.

## Overview

ExfilShield hooks into Windows’ device notification system to detect hardware arrivals and removals in real time. It classifies each connected device, logs the event, and can enforce rules such as blocking or restricting access based on vendor or device ID.

The goal is to act as an endpoint-level “data egress firewall” — lightweight, reliable, and silent — designed for system-level integration or corporate deployments.
