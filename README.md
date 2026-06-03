# Tlali

Repositorio general del proyecto Tlali, una plataforma web para monitoreo de invernadero inteligente.

Este repo contiene la documentacion general, archivos de entorno de ejemplo, configuraciones de Docker y referencias a los repos de backend y frontend como submodulos.

## Estructura

- `tlali-back`: API REST en Spring Boot, Java 17 y Maven.
- `tlali-front`: tablero web en React, Vite y Tailwind CSS.

## Clonar con submodulos

```powershell
git clone --recurse-submodules https://github.com/NahumAgp/Tlali.git
```

Si ya clonaste el repo sin submodulos:

```powershell
git submodule update --init --recursive
```

## Flujo de trabajo

Los cambios de backend y frontend se commitean y pushean dentro de su propio repo. Despues, el repo general guarda la nueva referencia del submodulo.

```powershell
cd tlali-back
git add .
git commit -m "Update backend"
git push

cd ..
git add tlali-back
git commit -m "Update backend submodule"
git push
```
