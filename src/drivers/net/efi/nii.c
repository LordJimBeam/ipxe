/*
 * Copyright (C) 2014 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <ipxe/netdevice.h>
#include <ipxe/ethernet.h>
#include <ipxe/umalloc.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_driver.h>
#include <ipxe/efi/efi_pci.h>
#include <ipxe/efi/efi_utils.h>
#include <ipxe/efi/Protocol/NetworkInterfaceIdentifier.h>
#include <ipxe/efi/IndustryStandard/Acpi10.h>
#include "nii.h"

/** @file
 *
 * NII driver
 *
 */

/* Error numbers generated by NII */
#define EIO_INVALID_CDB __einfo_error ( EINFO_EIO_INVALID_CDB )
#define EINFO_EIO_INVALID_CDB						      \
	__einfo_uniqify ( EINFO_EIO, PXE_STATCODE_INVALID_CDB,		      \
			  "Invalid CDB" )
#define EIO_INVALID_CPB __einfo_error ( EINFO_EIO_INVALID_CPB )
#define EINFO_EIO_INVALID_CPB						      \
	__einfo_uniqify ( EINFO_EIO, PXE_STATCODE_INVALID_CPB,		      \
			  "Invalid CPB" )
#define EIO_BUSY __einfo_error ( EINFO_EIO_BUSY )
#define EINFO_EIO_BUSY							      \
	__einfo_uniqify ( EINFO_EIO, PXE_STATCODE_BUSY,			      \
			  "Busy" )
#define EIO_QUEUE_FULL __einfo_error ( EINFO_EIO_QUEUE_FULL )
#define EINFO_EIO_QUEUE_FULL						      \
	__einfo_uniqify ( EINFO_EIO, PXE_STATCODE_QUEUE_FULL,		      \
			  "Queue full" )
#define EIO_ALREADY_STARTED __einfo_error ( EINFO_EIO_ALREADY_STARTED )
#define EINFO_EIO_ALREADY_STARTED					      \
	__einfo_uniqify ( EINFO_EIO, PXE_STATCODE_ALREADY_STARTED,	      \
			  "Already started" )
#define EIO_NOT_STARTED __einfo_error ( EINFO_EIO_NOT_STARTED )
#define EINFO_EIO_NOT_STARTED						      \
	__einfo_uniqify ( EINFO_EIO, PXE_STATCODE_NOT_STARTED,		      \
			  "Not started" )
#define EIO_NOT_SHUTDOWN __einfo_error ( EINFO_EIO_NOT_SHUTDOWN )
#define EINFO_EIO_NOT_SHUTDOWN						      \
	__einfo_uniqify ( EINFO_EIO, PXE_STATCODE_NOT_SHUTDOWN,		      \
			  "Not shutdown" )
#define EIO_ALREADY_INITIALIZED __einfo_error ( EINFO_EIO_ALREADY_INITIALIZED )
#define EINFO_EIO_ALREADY_INITIALIZED					      \
	__einfo_uniqify ( EINFO_EIO, PXE_STATCODE_ALREADY_INITIALIZED,	      \
			  "Already initialized" )
#define EIO_NOT_INITIALIZED __einfo_error ( EINFO_EIO_NOT_INITIALIZED )
#define EINFO_EIO_NOT_INITIALIZED					      \
	__einfo_uniqify ( EINFO_EIO, PXE_STATCODE_NOT_INITIALIZED,	      \
			  "Not initialized" )
#define EIO_DEVICE_FAILURE __einfo_error ( EINFO_EIO_DEVICE_FAILURE )
#define EINFO_EIO_DEVICE_FAILURE					      \
	__einfo_uniqify ( EINFO_EIO, PXE_STATCODE_DEVICE_FAILURE,	      \
			  "Device failure" )
#define EIO_NVDATA_FAILURE __einfo_error ( EINFO_EIO_NVDATA_FAILURE )
#define EINFO_EIO_NVDATA_FAILURE					      \
	__einfo_uniqify ( EINFO_EIO, PXE_STATCODE_NVDATA_FAILURE,	      \
			  "Non-volatile data failure" )
#define EIO_UNSUPPORTED __einfo_error ( EINFO_EIO_UNSUPPORTED )
#define EINFO_EIO_UNSUPPORTED						      \
	__einfo_uniqify ( EINFO_EIO, PXE_STATCODE_UNSUPPORTED,		      \
			  "Unsupported" )
#define EIO_BUFFER_FULL __einfo_error ( EINFO_EIO_BUFFER_FULL )
#define EINFO_EIO_BUFFER_FULL						      \
	__einfo_uniqify ( EINFO_EIO, PXE_STATCODE_BUFFER_FULL,		      \
			  "Buffer full" )
#define EIO_INVALID_PARAMETER __einfo_error ( EINFO_EIO_INVALID_PARAMETER )
#define EINFO_EIO_INVALID_PARAMETER					      \
	__einfo_uniqify ( EINFO_EIO, PXE_STATCODE_INVALID_PARAMETER,	      \
			  "Invalid parameter" )
#define EIO_INVALID_UNDI __einfo_error ( EINFO_EIO_INVALID_UNDI )
#define EINFO_EIO_INVALID_UNDI						      \
	__einfo_uniqify ( EINFO_EIO, PXE_STATCODE_INVALID_UNDI,		      \
			  "Invalid UNDI" )
#define EIO_IPV4_NOT_SUPPORTED __einfo_error ( EINFO_EIO_IPV4_NOT_SUPPORTED )
#define EINFO_EIO_IPV4_NOT_SUPPORTED					      \
	__einfo_uniqify ( EINFO_EIO, PXE_STATCODE_IPV4_NOT_SUPPORTED,	      \
			  "IPv4 not supported" )
#define EIO_IPV6_NOT_SUPPORTED __einfo_error ( EINFO_EIO_IPV6_NOT_SUPPORTED )
#define EINFO_EIO_IPV6_NOT_SUPPORTED					      \
	__einfo_uniqify ( EINFO_EIO, PXE_STATCODE_IPV6_NOT_SUPPORTED,	      \
			  "IPv6 not supported" )
#define EIO_NOT_ENOUGH_MEMORY __einfo_error ( EINFO_EIO_NOT_ENOUGH_MEMORY )
#define EINFO_EIO_NOT_ENOUGH_MEMORY					      \
	__einfo_uniqify ( EINFO_EIO, PXE_STATCODE_NOT_ENOUGH_MEMORY,	      \
			  "Not enough memory" )
#define EIO_NO_DATA __einfo_error ( EINFO_EIO_NO_DATA )
#define EINFO_EIO_NO_DATA						      \
	__einfo_uniqify ( EINFO_EIO, PXE_STATCODE_NO_DATA,		      \
			  "No data" )
#define EIO_STAT( stat )						      \
	EUNIQ ( EINFO_EIO, -(stat), EIO_INVALID_CDB, EIO_INVALID_CPB,	      \
		EIO_BUSY, EIO_QUEUE_FULL, EIO_ALREADY_STARTED,		      \
		EIO_NOT_STARTED, EIO_NOT_SHUTDOWN, EIO_ALREADY_INITIALIZED,   \
		EIO_NOT_INITIALIZED, EIO_DEVICE_FAILURE, EIO_NVDATA_FAILURE,  \
		EIO_UNSUPPORTED, EIO_BUFFER_FULL, EIO_INVALID_PARAMETER,      \
		EIO_INVALID_UNDI, EIO_IPV4_NOT_SUPPORTED,		      \
		EIO_IPV6_NOT_SUPPORTED, EIO_NOT_ENOUGH_MEMORY, EIO_NO_DATA )

/** Maximum PCI BAR
 *
 * This is defined in <ipxe/efi/IndustryStandard/Pci22.h>, but we
 * can't #include that since it collides with <ipxe/pci.h>.
 */
#define PCI_MAX_BAR 6

/** An NII NIC */
struct nii_nic {
	/** EFI device */
	struct efi_device *efidev;
	/** Network interface identifier protocol */
	EFI_NETWORK_INTERFACE_IDENTIFIER_PROTOCOL *nii;
	/** !PXE structure */
	PXE_SW_UNDI *undi;
	/** Entry point */
	EFIAPI VOID ( * issue ) ( UINT64 cdb );
	/** Generic device */
	struct device dev;

	/** PCI device */
	EFI_HANDLE pci_device;
	/** PCI I/O protocol */
	EFI_PCI_IO_PROTOCOL *pci_io;
	/** Memory BAR */
	unsigned int mem_bar;
	/** I/O BAR */
	unsigned int io_bar;

	/** Broadcast address */
	PXE_MAC_ADDR broadcast;
	/** Maximum packet length */
	size_t mtu;

	/** Hardware transmit/receive buffer */
	userptr_t buffer;
	/** Hardware transmit/receive buffer length */
	size_t buffer_len;

	/** Saved task priority level */
	EFI_TPL saved_tpl;

	/** Current transmit buffer */
	struct io_buffer *txbuf;
	/** Current receive buffer */
	struct io_buffer *rxbuf;
};

/** Maximum number of received packets per poll */
#define NII_RX_QUOTA 4

/**
 * Open PCI I/O protocol and identify BARs
 *
 * @v nii		NII NIC
 * @ret rc		Return status code
 */
static int nii_pci_open ( struct nii_nic *nii ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_HANDLE device = nii->efidev->device;
	EFI_HANDLE pci_device;
	union {
		EFI_PCI_IO_PROTOCOL *pci_io;
		void *interface;
	} pci_io;
	union {
		EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *acpi;
		void *resource;
	} desc;
	unsigned int bar;
	EFI_STATUS efirc;
	int rc;

	/* Locate PCI I/O protocol */
	if ( ( rc = efi_locate_device ( device, &efi_pci_io_protocol_guid,
					&pci_device ) ) != 0 ) {
		DBGC ( nii, "NII %s could not locate PCI I/O protocol: %s\n",
		       nii->dev.name, strerror ( rc ) );
		goto err_locate;
	}
	nii->pci_device = pci_device;

	/* Open PCI I/O protocol */
	if ( ( efirc = bs->OpenProtocol ( pci_device, &efi_pci_io_protocol_guid,
					  &pci_io.interface, efi_image_handle,
					  device,
					  EFI_OPEN_PROTOCOL_GET_PROTOCOL ))!=0){
		rc = -EEFI ( efirc );
		DBGC ( nii, "NII %s could not open PCI I/O protocol: %s\n",
		       nii->dev.name, strerror ( rc ) );
		goto err_open;
	}
	nii->pci_io = pci_io.pci_io;

	/* Identify memory and I/O BARs */
	nii->mem_bar = PCI_MAX_BAR;
	nii->io_bar = PCI_MAX_BAR;
	for ( bar = 0 ; bar < PCI_MAX_BAR ; bar++ ) {
		efirc = nii->pci_io->GetBarAttributes ( nii->pci_io, bar, NULL,
							&desc.resource );
		if ( efirc == EFI_UNSUPPORTED ) {
			/* BAR not present; ignore */
			continue;
		}
		if ( efirc != 0 ) {
			rc = -EEFI ( efirc );
			DBGC ( nii, "NII %s could not get BAR %d attributes: "
			       "%s\n", nii->dev.name, bar, strerror ( rc ) );
			goto err_get_bar_attributes;
		}
		if ( desc.acpi->ResType == ACPI_ADDRESS_SPACE_TYPE_MEM ) {
			nii->mem_bar = bar;
		} else if ( desc.acpi->ResType == ACPI_ADDRESS_SPACE_TYPE_IO ) {
			nii->io_bar = bar;
		}
		bs->FreePool ( desc.resource );
	}
	DBGC ( nii, "NII %s has ", nii->dev.name );
	if ( nii->mem_bar < PCI_MAX_BAR ) {
		DBGC ( nii, "memory BAR %d and ", nii->mem_bar );
	} else {
		DBGC ( nii, "no memory BAR and " );
	}
	if ( nii->io_bar < PCI_MAX_BAR ) {
		DBGC ( nii, "I/O BAR %d\n", nii->io_bar );
	} else {
		DBGC ( nii, "no I/O BAR\n" );
	}

	return 0;

 err_get_bar_attributes:
	bs->CloseProtocol ( pci_device, &efi_pci_io_protocol_guid,
			    efi_image_handle, device );
 err_open:
 err_locate:
	return rc;
}

/**
 * Close PCI I/O protocol
 *
 * @v nii		NII NIC
 * @ret rc		Return status code
 */
static void nii_pci_close ( struct nii_nic *nii ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;

	bs->CloseProtocol ( nii->pci_device, &efi_pci_io_protocol_guid,
			    efi_image_handle, nii->efidev->device );
}

/**
 * I/O callback
 *
 * @v unique_id		NII NIC
 * @v op		Operations
 * @v len		Length of data
 * @v addr		Address
 * @v data		Data buffer
 */
static EFIAPI VOID nii_io ( UINT64 unique_id, UINT8 op, UINT8 len, UINT64 addr,
			    UINT64 data ) {
	struct nii_nic *nii = ( ( void * ) ( intptr_t ) unique_id );
	EFI_PCI_IO_PROTOCOL_ACCESS *access;
	EFI_PCI_IO_PROTOCOL_IO_MEM io;
	EFI_PCI_IO_PROTOCOL_WIDTH width;
	unsigned int bar;
	EFI_STATUS efirc;
	int rc;

	/* Determine accessor and BAR */
	if ( op & ( PXE_MEM_READ | PXE_MEM_WRITE ) ) {
		access = &nii->pci_io->Mem;
		bar = nii->mem_bar;
	} else {
		access = &nii->pci_io->Io;
		bar = nii->io_bar;
	}

	/* Determine operaton */
	io = ( ( op & ( PXE_IO_WRITE | PXE_MEM_WRITE ) ) ?
	       access->Write : access->Read );

	/* Determine width */
	width = ( fls ( len ) - 1 );

	/* Issue operation */
	if ( ( efirc = io ( nii->pci_io, width, bar, addr, 1,
			    ( ( void * ) ( intptr_t ) data ) ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( nii, "NII %s I/O operation %#x failed: %s\n",
		       nii->dev.name, op, strerror ( rc ) );
		/* No way to report failure */
		return;
	}
}

/**
 * Delay callback
 *
 * @v unique_id		NII NIC
 * @v microseconds	Delay in microseconds
 */
static EFIAPI VOID nii_delay ( UINT64 unique_id __unused, UINTN microseconds ) {

	udelay ( microseconds );
}

/**
 * Block callback
 *
 * @v unique_id		NII NIC
 * @v acquire		Acquire lock
 */
static EFIAPI VOID nii_block ( UINT64 unique_id, UINT32 acquire ) {
	struct nii_nic *nii = ( ( void * ) ( intptr_t ) unique_id );
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;

	/* This functionality (which is copied verbatim from the
	 * SnpDxe implementation of this function) appears to be
	 * totally brain-dead, since it produces no actual blocking
	 * behaviour.
	 */
	if ( acquire ) {
		nii->saved_tpl = bs->RaiseTPL ( TPL_NOTIFY );
	} else {
		bs->RestoreTPL ( nii->saved_tpl );
	}
}

/**
 * Construct operation from opcode and flags
 *
 * @v opcode		Opcode
 * @v opflags		Flags
 * @ret op		Operation
 */
#define NII_OP( opcode, opflags ) ( (opcode) | ( (opflags) << 16 ) )

/**
 * Extract opcode from operation
 *
 * @v op		Operation
 * @ret opcode		Opcode
 */
#define NII_OPCODE( op ) ( (op) & 0xffff )

/**
 * Extract flags from operation
 *
 * @v op		Operation
 * @ret opflags		Flags
 */
#define NII_OPFLAGS( op ) ( (op) >> 16 )

/**
 * Issue command with parameter block and data block
 *
 * @v nii		NII NIC
 * @v op		Operation
 * @v cpb		Command parameter block, or NULL
 * @v cpb_len		Command parameter block length
 * @v db		Data block, or NULL
 * @v db_len		Data block length
 * @ret stat		Status flags, or negative status code
 */
static int nii_issue_cpb_db ( struct nii_nic *nii, unsigned int op, void *cpb,
			      size_t cpb_len, void *db, size_t db_len ) {
	PXE_CDB cdb;

	/* Prepare command descriptor block */
	memset ( &cdb, 0, sizeof ( cdb ) );
	cdb.OpCode = NII_OPCODE ( op );
	cdb.OpFlags = NII_OPFLAGS ( op );
	cdb.CPBaddr = ( ( intptr_t ) cpb );
	cdb.CPBsize = cpb_len;
	cdb.DBaddr = ( ( intptr_t ) db );
	cdb.DBsize = db_len;
	cdb.IFnum = nii->nii->IfNum;

	/* Issue command */
	nii->issue ( ( intptr_t ) &cdb );

	/* Check completion status */
	if ( cdb.StatCode != PXE_STATCODE_SUCCESS )
		return -cdb.StatCode;

	/* Return command-specific status flags */
	return ( cdb.StatFlags & ~PXE_STATFLAGS_STATUS_MASK );
}

/**
 * Issue command with parameter block
 *
 * @v nii		NII NIC
 * @v op		Operation
 * @v cpb		Command parameter block, or NULL
 * @v cpb_len		Command parameter block length
 * @ret stat		Status flags, or negative status code
 */
static int nii_issue_cpb ( struct nii_nic *nii, unsigned int op, void *cpb,
			   size_t cpb_len ) {

	return nii_issue_cpb_db ( nii, op, cpb, cpb_len, NULL, 0 );
}

/**
 * Issue command with data block
 *
 * @v nii		NII NIC
 * @v op		Operation
 * @v db		Data block, or NULL
 * @v db_len		Data block length
 * @ret stat		Status flags, or negative status code
 */
static int nii_issue_db ( struct nii_nic *nii, unsigned int op, void *db,
			  size_t db_len ) {

	return nii_issue_cpb_db ( nii, op, NULL, 0, db, db_len );
}

/**
 * Issue command
 *
 *
 * @v nii		NII NIC
 * @v op		Operation
 * @ret stat		Status flags, or negative status code
 */
static int nii_issue ( struct nii_nic *nii, unsigned int op ) {

	return nii_issue_cpb_db ( nii, op, NULL, 0, NULL, 0 );
}

/**
 * Start UNDI
 *
 * @v nii		NII NIC
 * @ret rc		Return status code
 */
static int nii_start_undi ( struct nii_nic *nii ) {
	PXE_CPB_START_31 cpb;
	int stat;
	int rc;

	/* Construct parameter block */
	memset ( &cpb, 0, sizeof ( cpb ) );
	cpb.Delay = ( ( intptr_t ) nii_delay );
	cpb.Block = ( ( intptr_t ) nii_block );
	cpb.Mem_IO = ( ( intptr_t ) nii_io );
	cpb.Unique_ID = ( ( intptr_t ) nii );

	/* Issue command */
	if ( ( stat = nii_issue_cpb ( nii, PXE_OPCODE_START, &cpb,
				      sizeof ( cpb ) ) ) < 0 ) {
		rc = -EIO_STAT ( stat );
		DBGC ( nii, "NII %s could not start: %s\n",
		       nii->dev.name, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Stop UNDI
 *
 * @v nii		NII NIC
 */
static void nii_stop_undi ( struct nii_nic *nii ) {
	int stat;
	int rc;

	/* Issue command */
	if ( ( stat = nii_issue ( nii, PXE_OPCODE_STOP ) ) < 0 ) {
		rc = -EIO_STAT ( stat );
		DBGC ( nii, "NII %s could not stop: %s\n",
		       nii->dev.name, strerror ( rc ) );
		/* Nothing we can do about it */
		return;
	}
}

/**
 * Get initialisation information
 *
 * @v nii		NII NIC
 * @v netdev		Network device to fill in
 * @ret rc		Return status code
 */
static int nii_get_init_info ( struct nii_nic *nii,
			       struct net_device *netdev ) {
	PXE_DB_GET_INIT_INFO db;
	int stat;
	int rc;

	/* Issue command */
	if ( ( stat = nii_issue_db ( nii, PXE_OPCODE_GET_INIT_INFO, &db,
				     sizeof ( db ) ) ) < 0 ) {
		rc = -EIO_STAT ( stat );
		DBGC ( nii, "NII %s could not get initialisation info: %s\n",
		       nii->dev.name, strerror ( rc ) );
		return rc;
	}

	/* Determine link layer protocol */
	switch ( db.IFtype ) {
	case PXE_IFTYPE_ETHERNET :
		netdev->ll_protocol = &ethernet_protocol;
		break;
	default:
		DBGC ( nii, "NII %s unknown interface type %#02x\n",
		       nii->dev.name, db.IFtype );
		return -ENOTSUP;
	}

	/* Sanity checks */
	assert ( db.MediaHeaderLen == netdev->ll_protocol->ll_header_len );
	assert ( db.HWaddrLen == netdev->ll_protocol->hw_addr_len );
	assert ( db.HWaddrLen == netdev->ll_protocol->ll_addr_len );

	/* Extract parameters */
	nii->buffer_len = db.MemoryRequired;
	nii->mtu = ( db.FrameDataLen + db.MediaHeaderLen );
	netdev->max_pkt_len = nii->mtu;

	return 0;
}

/**
 * Initialise UNDI
 *
 * @v nii		NII NIC
 * @ret rc		Return status code
 */
static int nii_initialise ( struct nii_nic *nii ) {
	PXE_CPB_INITIALIZE cpb;
	unsigned int op;
	int stat;
	int rc;

	/* Allocate memory buffer */
	nii->buffer = umalloc ( nii->buffer_len );
	if ( ! nii->buffer ) {
		rc = -ENOMEM;
		goto err_alloc;
	}

	/* Construct parameter block */
	memset ( &cpb, 0, sizeof ( cpb ) );
	cpb.MemoryAddr = ( ( intptr_t ) nii->buffer );
	cpb.MemoryLength = nii->buffer_len;

	/* Issue command */
	op = NII_OP ( PXE_OPCODE_INITIALIZE,
		      PXE_OPFLAGS_INITIALIZE_DO_NOT_DETECT_CABLE );
	if ( ( stat = nii_issue_cpb ( nii, op, &cpb, sizeof ( cpb ) ) ) < 0 ) {
		rc = -EIO_STAT ( stat );
		DBGC ( nii, "NII %s could not initialise: %s\n",
		       nii->dev.name, strerror ( rc ) );
		goto err_initialize;
	}

	return 0;

 err_initialize:
	ufree ( nii->buffer );
 err_alloc:
	return rc;
}

/**
 * Shut down UNDI
 *
 * @v nii		NII NIC
 */
static void nii_shutdown ( struct nii_nic *nii ) {
	int stat;
	int rc;

	/* Issue command */
	if ( ( stat = nii_issue ( nii, PXE_OPCODE_SHUTDOWN ) ) < 0 ) {
		rc = -EIO_STAT ( stat );
		DBGC ( nii, "NII %s could not shut down: %s\n",
		       nii->dev.name, strerror ( rc ) );
		/* Leak memory to avoid corruption */
		return;
	}

	/* Free buffer */
	ufree ( nii->buffer );
}

/**
 * Get station addresses
 *
 * @v nii		NII NIC
 * @v netdev		Network device to fill in
 * @ret rc		Return status code
 */
static int nii_get_station_address ( struct nii_nic *nii,
				     struct net_device *netdev ) {
	PXE_DB_STATION_ADDRESS db;
	int stat;
	int rc;

	/* Initialise UNDI */
	if ( ( rc = nii_initialise ( nii ) ) != 0 )
		goto err_initialise;

	/* Issue command */
	if ( ( stat = nii_issue_db ( nii, PXE_OPCODE_STATION_ADDRESS, &db,
				     sizeof ( db ) ) ) < 0 ) {
		rc = -EIO_STAT ( stat );
		DBGC ( nii, "NII %s could not get station address: %s\n",
		       nii->dev.name, strerror ( rc ) );
		goto err_station_address;
	}

	/* Copy MAC addresses */
	memcpy ( netdev->ll_addr, db.StationAddr,
		 netdev->ll_protocol->ll_addr_len );
	memcpy ( netdev->hw_addr, db.PermanentAddr,
		 netdev->ll_protocol->hw_addr_len );
	memcpy ( nii->broadcast, db.BroadcastAddr,
		 sizeof ( nii->broadcast ) );

 err_station_address:
	nii_shutdown ( nii );
 err_initialise:
	return rc;
}

/**
 * Set station address
 *
 * @v nii		NII NIC
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int nii_set_station_address ( struct nii_nic *nii,
				     struct net_device *netdev ) {
	PXE_CPB_STATION_ADDRESS cpb;
	int stat;
	int rc;

	/* Construct parameter block */
	memset ( &cpb, 0, sizeof ( cpb ) );
	memcpy ( cpb.StationAddr, netdev->ll_addr,
		 netdev->ll_protocol->ll_addr_len );

	/* Issue command */
	if ( ( stat = nii_issue_cpb ( nii, PXE_OPCODE_STATION_ADDRESS,
				      &cpb, sizeof ( cpb ) ) ) < 0 ) {
		rc = -EIO_STAT ( stat );
		DBGC ( nii, "NII %s could not set station address: %s\n",
		       nii->dev.name, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Set receive filters
 *
 * @v nii		NII NIC
 * @ret rc		Return status code
 */
static int nii_set_rx_filters ( struct nii_nic *nii ) {
	unsigned int op;
	int stat;
	int rc;

	/* Issue command */
	op = NII_OP ( PXE_OPCODE_RECEIVE_FILTERS,
		      ( PXE_OPFLAGS_RECEIVE_FILTER_ENABLE |
			PXE_OPFLAGS_RECEIVE_FILTER_UNICAST |
			PXE_OPFLAGS_RECEIVE_FILTER_BROADCAST |
			PXE_OPFLAGS_RECEIVE_FILTER_PROMISCUOUS |
			PXE_OPFLAGS_RECEIVE_FILTER_ALL_MULTICAST ) );
	if ( ( stat = nii_issue ( nii, op ) ) < 0 ) {
		rc = -EIO_STAT ( stat );
		DBGC ( nii, "NII %s could not set receive filters: %s\n",
		       nii->dev.name, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Transmit packet
 *
 * @v netdev		Network device
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int nii_transmit ( struct net_device *netdev,
			  struct io_buffer *iobuf ) {
	struct nii_nic *nii = netdev->priv;
	PXE_CPB_TRANSMIT cpb;
	int stat;
	int rc;

	/* Defer the packet if there is already a transmission in progress */
	if ( nii->txbuf ) {
		netdev_tx_defer ( netdev, iobuf );
		return 0;
	}

	/* Construct parameter block */
	memset ( &cpb, 0, sizeof ( cpb ) );
	cpb.FrameAddr = virt_to_bus ( iobuf->data );
	cpb.DataLen = iob_len ( iobuf );
	cpb.MediaheaderLen = netdev->ll_protocol->ll_header_len;

	/* Transmit packet */
	if ( ( stat = nii_issue_cpb ( nii, PXE_OPCODE_TRANSMIT, &cpb,
				      sizeof ( cpb ) ) ) < 0 ) {
		rc = -EIO_STAT ( stat );
		DBGC ( nii, "NII %s could not transmit: %s\n",
		       nii->dev.name, strerror ( rc ) );
		return rc;
	}
	nii->txbuf = iobuf;

	return 0;
}

/**
 * Poll for completed packets
 *
 * @v netdev		Network device
 * @v stat		Status flags
 */
static void nii_poll_tx ( struct net_device *netdev, unsigned int stat ) {
	struct nii_nic *nii = netdev->priv;
	struct io_buffer *iobuf;

	/* Do nothing unless we have a completion */
	if ( stat & PXE_STATFLAGS_GET_STATUS_NO_TXBUFS_WRITTEN )
		return;

	/* Sanity check */
	if ( ! nii->txbuf ) {
		DBGC ( nii, "NII %s reported spurious TX completion\n",
		       nii->dev.name );
		netdev_tx_err ( netdev, NULL, -EPIPE );
		return;
	}

	/* Complete transmission */
	iobuf = nii->txbuf;
	nii->txbuf = NULL;
	netdev_tx_complete ( netdev, iobuf );
}

/**
 * Poll for received packets
 *
 * @v netdev		Network device
 */
static void nii_poll_rx ( struct net_device *netdev ) {
	struct nii_nic *nii = netdev->priv;
	PXE_CPB_RECEIVE cpb;
	PXE_DB_RECEIVE db;
	unsigned int quota;
	int stat;
	int rc;

	/* Retrieve up to NII_RX_QUOTA packets */
	for ( quota = NII_RX_QUOTA ; quota ; quota-- ) {

		/* Allocate buffer, if required */
		if ( ! nii->rxbuf ) {
			nii->rxbuf = alloc_iob ( nii->mtu );
			if ( ! nii->rxbuf ) {
				/* Leave for next poll */
				break;
			}
		}

		/* Construct parameter block */
		memset ( &cpb, 0, sizeof ( cpb ) );
		cpb.BufferAddr = virt_to_bus ( nii->rxbuf->data );
		cpb.BufferLen = iob_tailroom ( nii->rxbuf );

		/* Issue command */
		if ( ( stat = nii_issue_cpb_db ( nii, PXE_OPCODE_RECEIVE,
						 &cpb, sizeof ( cpb ),
						 &db, sizeof ( db ) ) ) < 0 ) {

			/* PXE_STATCODE_NO_DATA is just the usual "no packet"
			 * status indicator; ignore it.
			 */
			if ( stat == -PXE_STATCODE_NO_DATA )
				break;

			/* Anything else is an error */
			rc = -EIO_STAT ( stat );
			DBGC ( nii, "NII %s could not receive: %s\n",
			       nii->dev.name, strerror ( rc ) );
			netdev_rx_err ( netdev, NULL, rc );
			break;
		}

		/* Hand off to network stack */
		iob_put ( nii->rxbuf, db.FrameLen );
		netdev_rx ( netdev, nii->rxbuf );
		nii->rxbuf = NULL;
	}
}

/**
 * Check for link state changes
 *
 * @v netdev		Network device
 * @v stat		Status flags
 */
static void nii_poll_link ( struct net_device *netdev, unsigned int stat ) {
	int no_media = ( stat & PXE_STATFLAGS_GET_STATUS_NO_MEDIA );

	if ( no_media && netdev_link_ok ( netdev ) ) {
		netdev_link_down ( netdev );
	} else if ( ( ! no_media ) && ( ! netdev_link_ok ( netdev ) ) ) {
		netdev_link_up ( netdev );
	}
}

/**
 * Poll for completed packets
 *
 * @v netdev		Network device
 */
static void nii_poll ( struct net_device *netdev ) {
	struct nii_nic *nii = netdev->priv;
	PXE_DB_GET_STATUS db;
	unsigned int op;
	int stat;
	int rc;

	/* Get status */
	op = NII_OP ( PXE_OPCODE_GET_STATUS,
		      ( PXE_OPFLAGS_GET_INTERRUPT_STATUS |
			PXE_OPFLAGS_GET_TRANSMITTED_BUFFERS |
			PXE_OPFLAGS_GET_MEDIA_STATUS ) );
	if ( ( stat = nii_issue_db ( nii, op, &db, sizeof ( db ) ) ) < 0 ) {
		rc = -EIO_STAT ( stat );
		DBGC ( nii, "NII %s could not get status: %s\n",
		       nii->dev.name, strerror ( rc ) );
		return;
	}

	/* Process any TX completions */
	nii_poll_tx ( netdev, stat );

	/* Process any RX completions */
	nii_poll_rx ( netdev );

	/* Check for link state changes */
	nii_poll_link ( netdev, stat );
}

/**
 * Open network device
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int nii_open ( struct net_device *netdev ) {
	struct nii_nic *nii = netdev->priv;
	int rc;

	/* Initialise NIC */
	if ( ( rc = nii_initialise ( nii ) ) != 0 )
		goto err_initialise;

	/* Attempt to set station address */
	if ( ( rc = nii_set_station_address ( nii, netdev ) ) != 0 ) {
		DBGC ( nii, "NII %s could not set station address: %s\n",
		       nii->dev.name, strerror ( rc ) );
		/* Treat as non-fatal */
	}

	/* Set receive filters */
	if ( ( rc = nii_set_rx_filters ( nii ) ) != 0 )
		goto err_set_rx_filters;

	return 0;

 err_set_rx_filters:
	nii_shutdown ( nii );
 err_initialise:
	return rc;
}

/**
 * Close network device
 *
 * @v netdev		Network device
 */
static void nii_close ( struct net_device *netdev ) {
	struct nii_nic *nii = netdev->priv;

	/* Shut down NIC */
	nii_shutdown ( nii );

	/* Discard transmit buffer, if applicable */
	if ( nii->txbuf ) {
		netdev_tx_complete_err ( netdev, nii->txbuf, -ECANCELED );
		nii->txbuf = NULL;
	}

	/* Discard receive buffer, if applicable */
	if ( nii->rxbuf ) {
		free_iob ( nii->rxbuf );
		nii->rxbuf = NULL;
	}
}

/** NII network device operations */
static struct net_device_operations nii_operations = {
	.open = nii_open,
	.close = nii_close,
	.transmit = nii_transmit,
	.poll = nii_poll,
};

/**
 * Attach driver to device
 *
 * @v efidev		EFI device
 * @ret rc		Return status code
 */
int nii_start ( struct efi_device *efidev ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_HANDLE device = efidev->device;
	struct net_device *netdev;
	struct nii_nic *nii;
	void *interface;
	EFI_STATUS efirc;
	int rc;

	/* Allocate and initialise structure */
	netdev = alloc_netdev ( sizeof ( *nii ) );
	if ( ! netdev ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	netdev_init ( netdev, &nii_operations );
	nii = netdev->priv;
	nii->efidev = efidev;
	netdev->ll_broadcast = nii->broadcast;
	efidev_set_drvdata ( efidev, netdev );

	/* Populate underlying device information */
	efi_device_info ( device, "NII", &nii->dev );
	nii->dev.driver_name = "NII";
	nii->dev.parent = &efidev->dev;
	list_add ( &nii->dev.siblings, &efidev->dev.children );
	INIT_LIST_HEAD ( &nii->dev.children );
	netdev->dev = &nii->dev;

	/* Open NII protocol */
	if ( ( efirc = bs->OpenProtocol ( device, &efi_nii31_protocol_guid,
					  &interface, efi_image_handle, device,
					  ( EFI_OPEN_PROTOCOL_BY_DRIVER |
					    EFI_OPEN_PROTOCOL_EXCLUSIVE )))!=0){
		rc = -EEFI ( efirc );
		DBGC ( nii, "NII %s cannot open NII protocol: %s\n",
		       nii->dev.name, strerror ( rc ) );
		DBGC_EFI_OPENERS ( device, device, &efi_nii31_protocol_guid );
		goto err_open_protocol;
	}
	nii->nii = interface;

	/* Locate UNDI and entry point */
	nii->undi = ( ( void * ) ( intptr_t ) nii->nii->Id );
	if ( ! nii->undi ) {
		DBGC ( nii, "NII %s has no UNDI\n", nii->dev.name );
		rc = -ENODEV;
		goto err_no_undi;
	}
	if ( nii->undi->Implementation & PXE_ROMID_IMP_HW_UNDI ) {
		DBGC ( nii, "NII %s is a mythical hardware UNDI\n",
		       nii->dev.name );
		rc = -ENOTSUP;
		goto err_hw_undi;
	}
	if ( nii->undi->Implementation & PXE_ROMID_IMP_SW_VIRT_ADDR ) {
		nii->issue = ( ( void * ) ( intptr_t ) nii->undi->EntryPoint );
	} else {
		nii->issue = ( ( ( void * ) nii->undi ) +
			       nii->undi->EntryPoint );
	}
	DBGC ( nii, "NII %s using UNDI v%x.%x at %p entry %p\n", nii->dev.name,
	       nii->nii->MajorVer, nii->nii->MinorVer, nii->undi, nii->issue );

	/* Open PCI I/O protocols and locate BARs */
	if ( ( rc = nii_pci_open ( nii ) ) != 0 )
		goto err_pci_open;

	/* Start UNDI */
	if ( ( rc = nii_start_undi ( nii ) ) != 0 )
		goto err_start_undi;

	/* Get initialisation information */
	if ( ( rc = nii_get_init_info ( nii, netdev ) ) != 0 )
		goto err_get_init_info;

	/* Get MAC addresses */
	if ( ( rc = nii_get_station_address ( nii, netdev ) ) != 0 )
		goto err_get_station_address;

	/* Register network device */
	if ( ( rc = register_netdev ( netdev ) ) != 0 )
		goto err_register_netdev;
	DBGC ( nii, "NII %s registered as %s for %p %s\n", nii->dev.name,
	       netdev->name, device, efi_handle_name ( device ) );

	return 0;

	unregister_netdev ( netdev );
 err_register_netdev:
 err_get_station_address:
 err_get_init_info:
	nii_stop_undi ( nii );
 err_start_undi:
	nii_pci_close ( nii );
 err_pci_open:
 err_hw_undi:
 err_no_undi:
	bs->CloseProtocol ( device, &efi_nii31_protocol_guid,
			    efi_image_handle, device );
 err_open_protocol:
	list_del ( &nii->dev.siblings );
	netdev_nullify ( netdev );
	netdev_put ( netdev );
 err_alloc:
	return rc;
}

/**
 * Detach driver from device
 *
 * @v efidev		EFI device
 */
void nii_stop ( struct efi_device *efidev ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	struct net_device *netdev = efidev_get_drvdata ( efidev );
	struct nii_nic *nii = netdev->priv;
	EFI_HANDLE device = efidev->device;

	/* Unregister network device */
	unregister_netdev ( netdev );

	/* Stop UNDI */
	nii_stop_undi ( nii );

	/* Close PCI I/O protocols */
	nii_pci_close ( nii );

	/* Close NII protocol */
	bs->CloseProtocol ( device, &efi_nii31_protocol_guid,
			    efi_image_handle, device );

	/* Free network device */
	list_del ( &nii->dev.siblings );
	netdev_nullify ( netdev );
	netdev_put ( netdev );
}
