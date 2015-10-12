var scene = new THREE.Scene();
var camera = new THREE.PerspectiveCamera( 60,
	window.innerWidth/window.innerHeight, 0.01, 2000 );

var renderer = new THREE.WebGLRenderer({ antialias: true });
renderer.setPixelRatio( window.devicePixelRatio );
renderer.setSize( window.innerWidth, window.innerHeight );
renderer.shadowMapEnabled = true;
renderer.shadowMapType = THREE.PCFShadowMap;
document.body.appendChild( renderer.domElement );

{
	var info = document.createElement( 'div' );
	info.style.position = 'absolute';
	info.style.top = '10px';
	info.style.width = '100%';
	info.style.textAlign = 'center';
	info.style.borderStyle = 'solid';
	info.innerHTML = 'Matt Test';
	document.body.appendChild( info );
}

function MakeTruss() {
	var radius = 0.5;
	var cont = new THREE.Object3D();
	var leg = new THREE.CylinderGeometry( radius, radius, 15, 32 );
	var span = new THREE.CylinderGeometry( radius, radius, 50, 32 );
	var seg = new THREE.CylinderGeometry( radius, radius, 100/6, 32 );
	var mtl1 = new THREE.MeshLambertMaterial( { color: 0x880000 } );
	var mtl2 = new THREE.MeshLambertMaterial( { color: 0x006600 } );
	var mtl3 = new THREE.MeshLambertMaterial( { color: 0x000077 } );

	var mesh;
	for( ix = 0; ix < 7; ++ix )
	{
		for( iz = 0; iz < 2; ++iz )
		{
			mesh = new THREE.Mesh( leg, mtl1 );
			mesh.castShadow = true;
			mesh.receiveShadow = true;
			mesh.position.set( (ix-3)*100/6, 15/2, (iz-0.5)*50 );
			cont.add( mesh );
			
			if( ix > 0 )
			{
				mesh = new THREE.Mesh( seg, mtl2 );
				mesh.castShadow = true;
				mesh.receiveShadow = true;
				mesh.position.set( (ix-3.5)*100/6, 15, (iz-0.5)*50 );
				mesh.rotation.z = Math.PI/2;
				cont.add( mesh );
			}
		}
		mesh = new THREE.Mesh( span, mtl3 );
		mesh.castShadow = true;
		mesh.receiveShadow = true;
		mesh.position.set( (ix-3)*100/6, 15, 0 );
		mesh.rotation.x = Math.PI/2;
		cont.add( mesh );
	}
	
	var geometry = new THREE.SphereGeometry( 0.5, 32, 32 );
	var material = new THREE.MeshBasicMaterial( {color: 0x00ffff} );
	var sphere = new THREE.Mesh( geometry, material );
	sphere.castShadow = true;
	sphere.receiveShadow = true;
	cont.add( sphere );
	return cont;
}

var truss = MakeTruss();
scene.add(truss);

function MakeWand() {
	var radius = 0.5/12;
	var cont = new THREE.Object3D();
	var geo = new THREE.CylinderGeometry( radius, radius, 3, 32 );
	var mtl = new THREE.MeshLambertMaterial( { color: 0xFFEEFF } );

	var mesh = new THREE.Mesh( geo, mtl );
	mesh.castShadow = true;
	mesh.receiveShadow = true;
	mesh.position.set( 0, 3/2, 0 );
	cont.add( mesh );
	
	geo = new THREE.SphereGeometry( radius*1.1, 32, 32 );
	mtl = new THREE.MeshBasicMaterial( {color: 0x00ffff} );
	mesh = new THREE.Mesh( geo, mtl );
	mesh.castShadow = true;
	mesh.receiveShadow = true;
	mesh.position.set( 0, 3, 0 );
	cont.add( mesh );

	return cont;
}

var wands = [];
for( i = 0; i < 100; ++i )
{
	var wand = MakeWand();
	wand.position.x = (Math.random() - 0.5) * 100;
	wand.position.y = (Math.random() * 10);
	wand.position.z = (Math.random() - 0.5) * 50;
	scene.add( wand );
	var o = { "vx":0, "vy":0, "vz":0 };
	o.mesh = wand;
	wands.push(o);
}

scene.add( new THREE.AmbientLight( 0x505050 ) );

var plane;
{
	var light = new THREE.SpotLight( 0xffffff, 1.5 );
	light.position.set( 0, 500, 2000 );
	light.castShadow = true;

	light.shadowCameraNear = 50;
	light.shadowCameraFar = camera.far;
	light.shadowCameraFov = 50;

	light.shadowBias = -0.00022;
	light.shadowDarkness = 0.5;

	light.shadowMapWidth = 2048;
	light.shadowMapHeight = 2048;

	scene.add( light );
	
	plane = new THREE.Mesh(
		new THREE.PlaneBufferGeometry( 2000, 2000, 8, 8 ),
		new THREE.MeshBasicMaterial( { color: 0xaaaaaa, opacity: 0.75, transparent: true } )
	);
	plane.visible = false;
	plane.rotation.x = -Math.PI/2;
	scene.add( plane );
	
	var gridHelper = new THREE.GridHelper( 100, 1 );
	scene.add( gridHelper );
}

var q = new THREE.Quaternion();
q.setFromAxisAngle( (new THREE.Vector3( 1, 1, 0 )).normalize(), Math.PI / 64 );

camera.position.set( 0, 50, 50 );
camera.rotation.x = -Math.PI/4;

controls = new THREE.TrackballControls( camera );
controls.rotateSpeed = 1.0;
controls.zoomSpeed = 2.0;
controls.panSpeed = 1.0;
controls.noZoom = false;
controls.noPan = false;
controls.staticMoving = true;
controls.dynamicDampingFactor = 0.25;

//var vx = 0, vy = 0, vz = 0;
var dq = new THREE.Quaternion();
var euler = new THREE.Euler(0, 0, 0);

var renderEnable = true;
var render = function () {
	if( !renderEnable )
		return;
	
	requestAnimationFrame( render );

	//cube.rotation.x += 0.1;
	//cube.rotation.y += 0.1;
	//truss.quaternion.copy( truss.quaternion.multiply(q) );
	//cube.updateMatrix();
	//console.log( renderer.
	//renderer.setSize( window.innerWidth, window.innerHeight );

	for( o of wands )
	{
		o.vx += (Math.random() - 0.5)*0.01;
		o.vy += (Math.random() - 0.5)*0.01;
		o.vz += (Math.random() - 0.5)*0.01;
		euler.set(o.vx,o.vy,o.vz);
		dq.setFromEuler( euler );
		dq.normalize();

		var wand = o.mesh;
		wand.quaternion.copy( wand.quaternion.multiply(dq) );
		// wand.rotation.x += vx;
		// wand.rotation.y += vy;
		// wand.rotation.z += vz;
	}

	controls.update();
	
	renderer.render(scene, camera);
};

render();

window.addEventListener('resize', function(event) {
	//console.log("Resizing");
	camera.aspect = window.innerWidth/window.innerHeight;
	camera.updateProjectionMatrix();
	renderer.setSize( window.innerWidth, window.innerHeight )				
});