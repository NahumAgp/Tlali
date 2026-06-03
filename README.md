# Tlali

Repositorio general del proyecto Tlali, una plataforma web para monitoreo de invernadero inteligente.

Este repo contiene la documentacion general, archivos de entorno de ejemplo, configuraciones de Docker y referencias a los repos de backend y frontend como submodulos.

## Estructura

- `tlali-back`: API REST en Spring Boot, Java 17 y Maven.
- `tlali-front`: tablero web en React, Vite y Tailwind CSS.

## Seguridad

Tlali incluye login local con JWT, Google OAuth y un usuario superadmin inicial para desarrollo.

```text
Correo: superadmin@tlali.local
Password: SuperAdmin123!
```

Antes de produccion cambia `TLALI_SUPERADMIN_PASSWORD` y `TLALI_JWT_SECRET`.

## Ejecutar en local

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
