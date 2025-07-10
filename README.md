# Updater<a name="EN-US_TOPIC_0000001148614629"></a>

-   [Introduction](#section184mcpsimp)
-   [Directory Structure](#section198mcpsimp)
-   [Usage](#section218mcpsimp)
    -   [Usage Guidelines](#section220mcpsimp)

-   [Repositories Involved](#section247mcpsimp)

## Introduction<a name="section184mcpsimp"></a>

The updater runs in the updater partition. It reads the misc partition information to obtain the update package status and verifies the update package to ensure that the update package is valid. Then, the updater parses the executable program from the update package, creates a subprocess, and starts the update program. After that, update operations will be automatically implemented by the update script.

## Directory Structure<a name="section198mcpsimp"></a>

```
base/update/updater/
├── resources           # UI image resources of the update subsystem
├── services            # Service code of the updater
│   ├── applypatch      # Update package data update code
│   ├── fs_manager      # File system and partition management code
│   ├── include         # Header files for the update subsystem
│   ├── log             # Log module of the update subsystem
│   ├── package         # Update packages
│   ├── script          # Update scripts
│   ├── diffpatch       # Differential package restore code
│   ├── sparse_image    # Sparse image parsing code
│   ├── ui              # UI code
│   └── updater_binary  # Executable programs
├── interfaces
│   └── kits            # External APIs
└── utils               # Common utilities of the update subsystem
    └── include         # Header files for general functions of the update subsystem
```

## Usage<a name="section218mcpsimp"></a>

### Usage Guidelines<a name="section220mcpsimp"></a>

The updater runs in the updater partition. To ensure proper functioning of the updater, perform the following operations:

1. Create a updater partition. 

The updater partition is independent of other partitions. It is recommended that the size of the updater partition be greater than or equal to 20 MB. The updater partition image is an ext4 file system. Ensure that the  **config**  option of the ext4 file system in the system kernel is enabled.

2. Create the misc partition.

The misc partition stores metadata required by the update subsystem during the update process. Such data includes update commands and records of resumable data transfer upon power-off. This partition is a raw partition and its size is about 1 MB. You do not need to create a file system for the misc partition, because the update subsystem can directly access this partition.

3. Prepare the partition configuration table.

During the update process, the updater needs to operate the partitions through the partition configuration table. The default file name of the partition configuration table is  **fstab.updater**. You need to pack the  **fstab.updater**  file into the updater during compilation.

4. Start the updater.

The init process in the updater partition has an independent configuration file named  **init.cfg**. The startup configuration of the updater is stored in this file.

5. Compile the updater.

a. Add the updater configurations to the  **build/subsystem\_config.json**  file.

Example configuration:

```
"updater": {
"project": "hmf/updater",
"path": "base/update/updater",
"name": "updater",
"dir": "base/update"
},
```

b. Add the updater for the desired product.

For example, to add the updater for Hi3516D V300, add the following code to the  **productdefine/common/products/Hi3516DV300.json**  file.

```
     "updater:updater":{},
```

6. Compile the updater partition image.

Add the compilation configuration to the  **build\_updater\_image.sh**  script, which is stored in the  **build**  repository and called by the OpenHarmony compilation system.

## AB Streaming Update<a name="section218mcpsimp"></a>

User devices may not always have sufficient space on the data partition for downloading OTA upgrade packages. Furthermore, entering the microsystem for upgrades sometimes impacts the user experience. To resolve these issues, OpenHarmony has introduced support for AB Streaming Updates. This feature allows downloading data chunks (Chunks) to be written directly to the inactive B partition, eliminating the need to store the chunks or the full update package on the data partition. Simultaneously, users can continue operating their devices on the active A partition while the update is applied to the inactive B partition, bypassing the need to enter the updater microsystem.

### Key Technologies<a name="section220mcpsimp"></a>

1、 Supports independent AB partition upgrades, enabling seamless updates.

2、 Supports resumable streaming updates, allowing updates to continue after interruptions like network disconnections or user pauses.

3、 Supports multiple security checks, including hash verification for each data chunk and integrity verification for each image after the update.

4、 Supports exception rollback, ensuring the device automatically reverts to the stable partition upon upgrade failure for a consistent user experience.

5、 Supports customizable streaming data chunk size, adjustable based on the device's data partition capacity.

### Packaging Process<a name="section220mcpsimp"></a>

The AB Streaming Update modifies the packaging of standard AB differential or full OTA packages. new and diff operations larger than the specified data size requirement in this document are split into smaller chunks. All delta dependency files are then packaged into the update.bin file using the TLV format.

1、 Split diff/new content exceeding 45KB

Input parameters: pkgdiff, new commands.

​Output: For the pkgdiff command, attempt to generate a diff file. If the diff file is larger than 45KB, split the pkgdiff operation into multiple smaller pkgdiff commands until each generated diff file is under 45KB. For the new command, directly check the block content size. Split any block not meeting the size requirement into multiple new commands.

2、​Calculate img blocks excluding those operated on in the action list

Input parameters: All blocks operated on in the action list, Total blocks of the img.

Output: The complement set of the two. Sort the required copy blocks in ascending order. Group consecutive blocks into sets. Combine them into copy commands using the standard new command format.

3、Package action, diff, etc., into update.bin per TLV format

Input parameters: transfer.list, new.dat, patch.dat.

Output: Package each line of the transfer.list as a chunk. Locate the dependent content in new.dat and patch.dat. Generate update.bin in TLV format.

## Repositories Involved<a name="section247mcpsimp"></a>

Update subsystem

[**update\_updater**](https://gitee.com/openharmony/update_updater)

[build](https://gitee.com/openharmony/build)

[productdefine\_common](https://gitee.com/openharmony/productdefine_common)

