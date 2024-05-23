#include <stdio.h>
#include <math.h>

//#define CNFGOGL
#define CNFG_IMPLEMENTATION
#include "rawdraw_sf.h"

#undef EXTERN_C

#include "openvr_capi.h"

#include "d3d11.h"

// If you know what area you want to copy...
#define ONLY_COPY_SUBRESOURCE

// For OpenVR
intptr_t VR_InitInternal( EVRInitError *peError, EVRApplicationType eType );
void VR_ShutdownInternal();
bool VR_IsHmdPresent();
intptr_t VR_GetGenericInterface( const char *pchInterfaceVersion, EVRInitError *peError );
bool VR_IsRuntimeInstalled();
const char * VR_GetVRInitErrorAsSymbol( EVRInitError error );
const char * VR_GetVRInitErrorAsEnglishDescription( EVRInitError error );

// Required for rawdraw
void HandleKey( int keycode, int bDown ) { }
void HandleButton( int x, int y, int button, int bDown ) { }
void HandleMotion( int x, int y, int mask ) { }
int HandleDestroy() { return 0; }

// This function was copy-pasted from cnovr.
void * GetOpenVRFunctionTable( const char * interfacename )
{
	EVRInitError e;
	char fnTableName[128];
	int result1 = snprintf( fnTableName, 128, "FnTable:%s", interfacename );
	void * ret = (void *)VR_GetGenericInterface( fnTableName, &e );
	printf( "Getting System FnTable: %s = %p (%d)\n", fnTableName, ret, e );
	if( !ret )
	{
		exit( 1 );
	}
	return ret;
}

#define CAPTURE_W 1024
#define CAPTURE_H 512

// buffer we will copy into
uint32_t rbuff[CAPTURE_W*CAPTURE_H];

// If we don't flip the bit order, it'll show up as BGRA instead of RGBA
uint32_t rgb_endian_flip( uint32_t r )
{
	return (r & 0xff00ff00) | ((r&0xff)<<16) | ((r&0xff0000)>>16);
}

int main()
{
	// Initialize OpenVR - looks like we need to call ourselves an overlay app to get the proper access.
	EVRInitError eError = 0;
	uint32_t token = VR_InitInternal( &eError, EVRApplicationType_VRApplication_Overlay );
	if( !token || eError )
	{
		fprintf( stderr, "Error: can't init OpenVR\n" );
		return -1;
	}

	// Get the compositor handle.
	struct VR_IVRCompositor_FnTable * oCompositor = GetOpenVRFunctionTable( IVRCompositor_Version );
	if( !oCompositor )
	{
		fprintf( stderr, "Error: can't get OpenVR interface\n" );
		return -2;
	}

	// Create the debug window
	if( CNFGSetup( "Texture Cap Test", CAPTURE_W, CAPTURE_H ) )
	{
		fprintf( stderr, "Error: Could not open debug window\n" );
		return -3;
	}

	// Just for debug
	char cts[1024];
	int frameno = 0;

	// Create a DirectX Device
	ID3D11Device * pDevice;
	ID3D11DeviceContext * pDeviceContext;
	enum D3D_FEATURE_LEVEL pFeatureLevels[1] = { D3D_FEATURE_LEVEL_11_0 };
	enum D3D_FEATURE_LEVEL pOutFeatureLevels;
	HRESULT hr;
	hr = D3D11CreateDevice(
		0,
		D3D_DRIVER_TYPE_HARDWARE,
		0,
		0,
		0,
		0,
		D3D11_SDK_VERSION,
		&pDevice,
		&pOutFeatureLevels,
		&pDeviceContext );

	ID3D11DeviceVtbl * pDeviceFn = pDevice->lpVtbl;

	printf( "D3D11CreateDevice: %08x %p %p\n", hr, pDeviceContext, pDevice );

	ID3D11Texture2D * texture2DID = 0;
	
	// Main logic loop.
	while( CNFGHandleInput() )
	{
		char * ctsbuff = cts;
		CNFGClearFrame();
		
		CNFGPenX = 50;
		CNFGPenY = 0;
		
		CNFGColor( 0xffffffff );
		
		// Get the Shader Resource View from OpenVR
		ID3D11ShaderResourceView * pD3D11ShaderResourceView;
		EVRCompositorError ce = oCompositor->GetMirrorTextureD3D11( EVREye_Eye_Left, pDevice, (void**)&pD3D11ShaderResourceView );
		ID3D11ShaderResourceViewVtbl* srvVT = pD3D11ShaderResourceView->lpVtbl;

		// Get a Texture2D from that resource view.
		ID3D11Texture2D * resource = 0;
		srvVT->GetResource( pD3D11ShaderResourceView, &resource );
        D3D11_TEXTURE2D_DESC desc2d;
		resource->lpVtbl->GetDesc( resource, &desc2d);

		ctsbuff += sprintf( ctsbuff, "Tex In: %d %d %d\n", desc2d.Width, desc2d.Height, desc2d.Format );
		
		// We have to generate our texture based off of the pixel format of the OpenVR Backing texture.
		if( texture2DID == 0 )
		{
			D3D11_TEXTURE2D_DESC texDesc;
			ZeroMemory(&texDesc, sizeof(D3D11_TEXTURE2D_DESC));
			printf( "Allocating %d x %d / %d\n", desc2d.Width, desc2d.Height, desc2d.Format );
#ifdef ONLY_COPY_SUBRESOURCE
			texDesc.Width = CAPTURE_W;
			texDesc.Height = CAPTURE_H;
#else
			texDesc.Width = desc2d.Width;
			texDesc.Height = desc2d.Height;
#endif
			texDesc.Format = desc2d.Format;
			texDesc.Usage = D3D11_USAGE_STAGING;
			texDesc.SampleDesc.Count = 1;
			texDesc.SampleDesc.Quality = 0;
			texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			texDesc.ArraySize = 1;
			texDesc.BindFlags = 0;
			texDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
			texDesc.MipLevels = 1;
			printf( "Creating Texture %p\n", pDeviceFn->CreateTexture2D );
			hr = pDeviceFn->CreateTexture2D( pDevice, &texDesc, NULL, &texture2DID);
			printf( "HR: %08x DEV: %08x\n", hr, texture2DID );
		}

		// Copy the OpenVR texture into our texture.
		int w = CAPTURE_W;
		int h = CAPTURE_H;
#ifdef ONLY_COPY_SUBRESOURCE
		D3D11_BOX src_box = { 0, 0, 0, CAPTURE_W, CAPTURE_H, 1 };
		pDeviceContext->lpVtbl->CopySubresourceRegion( pDeviceContext, texture2DID, 0, 0, 0, 0, resource, 0, &src_box );
#else
		pDeviceContext->lpVtbl->CopyResource( pDeviceContext, texture2DID, resource );
#endif

		// Release the texture back to OpenVR ASAP
		oCompositor->ReleaseMirrorTextureD3D11( pD3D11ShaderResourceView );
		
		
		// We can then process through that pixel data, after we map our texture that we copied into.
		D3D11_MAPPED_SUBRESOURCE mappedResource = { 0 };
		hr = pDeviceContext->lpVtbl->Map( pDeviceContext, texture2DID, 0, D3D11_MAP_READ, 0, &mappedResource );
		ctsbuff += sprintf( ctsbuff, "MAPPED: %p / HR: %08x (%d %d)\n", mappedResource.pData, hr, mappedResource.RowPitch, mappedResource.DepthPitch );
		int x, y;
		int z;
		uint8_t * dataptr = mappedResource.pData;

		if( mappedResource.pData )
		{
			for( y = 0; y < h && y < desc2d.Height; y++ )
			{
				int mw = (w<desc2d.Height)?w:desc2d.Height;
				for( x = 0; x < mw; x++ )
				{
					rbuff[x+y*CAPTURE_W] = rgb_endian_flip( ((uint32_t*)( ((uint8_t*)mappedResource.pData) + mappedResource.RowPitch * y ))[x] );
				}
			}
		}
		
		// Draw the block we wrote into to the screen.
		CNFGBlitImage( rbuff, 0, 0, w, h );
		
		// Cleanup
		pDeviceContext->lpVtbl->Unmap( pDeviceContext, texture2DID, 0 );
		
		resource->lpVtbl->Release( resource );
		
		// Write the rest of our debug data
		ctsbuff += sprintf( ctsbuff, "%d\n", frameno );
		CNFGDrawText( cts, 2 );
		
		CNFGSwapBuffers();
		frameno++;
	}
	return 0;
}
