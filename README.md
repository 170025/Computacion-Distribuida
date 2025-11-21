# Computacion-Distribuida
## Proyecto JimenezA-clienteFTP

Se logro implementar los comandos especificados dentro de RFC 959.

Para la ejecucion del programa, se trabajo en un ambiente local por ende para conectarse se especifico la direccion IP 127.0.0.1
y el puerto 21.

Al momento de realizar el Login es necesario hacerlo especificando los comandos, primero colocamos "USER <nombre-usuario>" y
a continuacion nos pedira que especifiquemos la contraseña, esto lo haremos con el comando "PASS <contraseña>" una vez realicemos esto
deberiamos poder realizar el Login de forma correcta y ahi si poder probar los comando dentro del servidor donde se haya conectado 
nuestro cliente. 

Para el manejo de concurrencia, el programa utiliza procesos mediante la funcion de fork(), creando diferentes hijos se puede realizar 
la transferencia de archivos en ambos sentidos y mantener la conexion activa con el servidor.
