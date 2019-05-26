Write-Output "current workdir: ${PWD}"
docker run --rm -ti -v ${PWD}:/opt/workspace totosan/iot_dev:latest