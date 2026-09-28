# Tlali Tlapixqui

Repositorio general del proyecto Tlali Tlapixqui, una plataforma web para monitoreo inteligente de sensores ambientales y de suelo.

Este repo contiene la documentacion general, archivos de entorno de ejemplo, configuraciones de Docker y referencias a los repos de backend y frontend como submodulos.

## Estructura

- `tlali-back`: API REST en Spring Boot, Java 17 y Maven.
- `tlali-front`: tablero web en React, Vite y Tailwind CSS.

## Seguridad

Tlali Tlapixqui incluye login local con JWT, Google OAuth y un usuario superadmin inicial para desarrollo.

```text
Correo: nahum.aguilar.per@gmail.com
Password: Admin123!
```

Antes de produccion cambia `TLALI_SUPERADMIN_PASSWORD` y `TLALI_JWT_SECRET`.

## Despliegue en Docker Swarm

El archivo `docker-stack.yml` despliega Tlali detrás de Traefik en la red
externa `VanaNet`. Docker Swarm no construye imágenes, por lo que primero se
deben construir el backend y el frontend desde el código descargado de GitHub.

```bash
git pull --ff-only
git submodule update --init --recursive
docker build -t tlali-backend:latest ./tlali-back
docker build -t tlali-frontend:latest ./tlali-front
docker compose --env-file .env -f docker-stack.yml config > .stack.rendered.yml
sed -i '/^[[:space:]]*name: /d' .stack.rendered.yml
docker stack config -c .stack.rendered.yml > /dev/null
docker stack deploy --prune -c .stack.rendered.yml tlali
rm -f .stack.rendered.yml
```

Define `TLALI_HOST` en `.env` para usar un dominio distinto al host HTTPS
temporal incluido en el stack. El servidor debe conservar `.env` fuera de Git.

## Ejecutar en local

### Con Docker Compose

La forma mas simple para correr Tlali completo en esta PC es con Docker Desktop:

```powershell
copy .env.example .env
docker compose up --build -d
```

Servicios:

- Frontend: `http://localhost:5173`
- Backend: `http://localhost:8080`
- MySQL: disponible solo dentro de Docker como `mysql:3306`

Credenciales locales por defecto:

```text
Correo: nahum.aguilar.per@gmail.com
Password: Admin123!
```

Para ver estado y logs:

```powershell
docker compose ps
docker compose logs -f backend
docker compose logs -f frontend
```

Para abrir una consola MySQL dentro del contenedor:

```powershell
docker compose exec mysql mysql -utlali -ptlali tlali
```

Para apagar sin borrar datos:

```powershell
docker compose down
```

Para apagar y borrar la base local de MySQL:

```powershell
docker compose down -v
```

### Sin Docker

Supabase local usa Docker para levantar Postgres, Auth, API y Studio. Primero abre Docker Desktop y espera a que el motor este corriendo.

```powershell
npm install
npx supabase start
```

Luego ejecuta el backend:

```powershell
cd tlali-back
.\mvnw.cmd spring-boot:run
```

En otra terminal ejecuta el frontend:

```powershell
cd tlali-front
npx --yes pnpm@10 install
node_modules\.bin\vite.CMD --host 127.0.0.1
```

URLs:

- Backend: `http://localhost:8080`
- Frontend: `http://127.0.0.1:5173`
- Supabase API local: `http://127.0.0.1:54321`
- Supabase Studio local: `http://127.0.0.1:54323`
- Supabase Postgres local: `localhost:54322`

Para revisar el estado de Supabase:

```powershell
npx supabase status
```

Para apagar Supabase local:

```powershell
npx supabase stop
```

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
